#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "PrecisionInfo.h"
#include "PropagationEngine.h"
#include "Utils.h"

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

static llvm::cl::OptionCategory Category(
    "precision-demotion-tool");

static llvm::cl::opt<double> OutputTolerance(
    "tolerance",
    llvm::cl::desc("Allowed absolute output error for demotion decisions"),
    llvm::cl::init(1e-2),
    llvm::cl::cat(Category));

static llvm::cl::opt<std::string> OutputFile(
    "output",
    llvm::cl::desc("Path where the rewritten demoted source is saved"),
    llvm::cl::init("../temp/demoted.cpp"),
    llvm::cl::cat(Category));

static llvm::cl::opt<std::string> AstDotFile(
    "ast-dot",
    llvm::cl::desc("Optional path where a Graphviz DOT expression AST is saved"),
    llvm::cl::init(""),
    llvm::cl::cat(Category));

static Rewriter Rewrite;

std::map<std::string, PrecisionInfo>
precisionTable;

struct FlowNode {
    std::string name;
    std::string rhs;
    SourceLocation typeLoc;
    double localError = 0.0;
    double requiredTolerance = 0.0;
    PrecisionType assignedType = FP32;
    bool rewriteAllowed = true;
    std::set<std::string> dependencies;
};

static std::vector<FlowNode>
flowNodes;

static std::set<std::string>
returnDependencies;

static double
returnExpressionError = 0.0;

static std::vector<std::string>
astDotStatements;

static unsigned
astDotNodeCounter = 0;

static std::string printExpr(
    const Expr *expr,
    ASTContext &context) {

    std::string text;
    llvm::raw_string_ostream out(text);

    expr->printPretty(
        out,
        nullptr,
        PrintingPolicy(context.getLangOpts()));

    out.flush();

    return text;
}

static std::string dotEscape(
    const std::string &text) {

    std::string escaped;

    for(char ch : text) {

        if(ch == '\\' || ch == '"')
            escaped.push_back('\\');

        if(ch == '\n')
            escaped += "\\n";
        else
            escaped.push_back(ch);
    }

    return escaped;
}

static std::string makeAstDotNode(
    const std::string &label) {

    std::string id =
        "n" + std::to_string(astDotNodeCounter++);

    astDotStatements.push_back(
        "    " + id
        + " [label=\""
        + dotEscape(label)
        + "\"];");

    return id;
}

static std::string appendExprAstDot(
    const Expr *expr,
    ASTContext &context);

static void appendAstEdge(
    const std::string &parent,
    const std::string &child) {

    astDotStatements.push_back(
        "    " + parent
        + " -> "
        + child
        + ";");
}

static std::string appendStmtAstDot(
    const Stmt *stmt,
    ASTContext &context) {

    if(!stmt)
        return makeAstDotNode("(null)");

    if(const Expr *expr =
        dyn_cast<Expr>(stmt))
        return appendExprAstDot(
            expr,
            context);

    return makeAstDotNode(
        stmt->getStmtClassName());
}

static std::string appendExprAstDot(
    const Expr *expr,
    ASTContext &context) {

    if(!expr)
        return makeAstDotNode("(null)");

    expr =
        expr->IgnoreParenImpCasts();

    if(const BinaryOperator *op =
        dyn_cast<BinaryOperator>(expr)) {

        std::string parent =
            makeAstDotNode(
                op->getOpcodeStr().str());

        appendAstEdge(
            parent,
            appendExprAstDot(
                op->getLHS(),
                context));

        appendAstEdge(
            parent,
            appendExprAstDot(
                op->getRHS(),
                context));

        return parent;
    }

    if(const UnaryOperator *op =
        dyn_cast<UnaryOperator>(expr)) {

        std::string parent =
            makeAstDotNode(
                UnaryOperator::getOpcodeStr(
                    op->getOpcode()).str());

        appendAstEdge(
            parent,
            appendExprAstDot(
                op->getSubExpr(),
                context));

        return parent;
    }

    if(const DeclRefExpr *ref =
        dyn_cast<DeclRefExpr>(expr))
        return makeAstDotNode(
            ref->getDecl()
                ? ref->getDecl()->getNameAsString()
                : printExpr(expr, context));

    if(isa<FloatingLiteral>(expr)
        || isa<IntegerLiteral>(expr))
        return makeAstDotNode(
            printExpr(expr, context));

    if(const CallExpr *call =
        dyn_cast<CallExpr>(expr)) {

        std::string label = "call";

        if(const FunctionDecl *callee =
            call->getDirectCallee())
            label = callee->getNameAsString() + "()";

        std::string parent =
            makeAstDotNode(label);

        for(const Expr *arg : call->arguments())
            appendAstEdge(
                parent,
                appendExprAstDot(
                    arg,
                    context));

        return parent;
    }

    std::string parent =
        makeAstDotNode(
            expr->getStmtClassName());

    for(const Stmt *child : expr->children()) {

        if(!child)
            continue;

        appendAstEdge(
            parent,
            appendStmtAstDot(
                child,
                context));
    }

    return parent;
}

static void addAstDotExpression(
    const std::string &title,
    const Expr *expr,
    ASTContext &context) {

    if(AstDotFile.empty() || !expr)
        return;

    astDotStatements.push_back(
        "    subgraph cluster_"
        + std::to_string(astDotNodeCounter++)
        + " {");

    astDotStatements.push_back(
        "        label=\""
        + dotEscape(title)
        + "\";");

    std::string root =
        appendExprAstDot(
            expr,
            context);

    astDotStatements.push_back(
        "        " + root
        + " [style=\"bold,filled\", fillcolor=\"white\"];");

    astDotStatements.push_back("    }");
}

static void writeAstDotFile() {

    if(AstDotFile.empty())
        return;

    llvm::SmallString<256>
    outPath(AstDotFile);

    llvm::sys::fs::create_directories(
        llvm::sys::path::parent_path(outPath));

    std::error_code EC;

    llvm::raw_fd_ostream out(
        AstDotFile,
        EC);

    if(EC) {

        llvm::errs()
            << "Failed to open AST DOT file "
            << AstDotFile
            << ": "
            << EC.message()
            << "\n";

        return;
    }

    out
        << "digraph ExpressionAST {\n"
        << "    graph [rankdir=TB, splines=line, nodesep=0.45, ranksep=0.55];\n"
        << "    node [shape=box, style=filled, fillcolor=white, color=\"#555555\", "
        << "fontname=\"Helvetica\", fontcolor=\"blue\"];\n"
        << "    edge [color=\"black\", arrowsize=0.6];\n";

    for(const std::string &statement : astDotStatements)
        out << statement << "\n";

    out << "}\n";
    out.close();

    std::cout
        << "\n[AST GRAPH]\n"
        << "Saved DOT to:\n"
        << AstDotFile
        << "\n";
}

static bool isFloatingCarrier(
    QualType type) {

    if(type->isRealFloatingType())
        return true;

    if(type->isPointerType()
        && type->getPointeeType()
            ->isRealFloatingType())
        return true;

    if(type->isArrayType()) {

        const ArrayType *arrayType =
            type->getAsArrayTypeUnsafe();

        return
            arrayType
            && arrayType->getElementType()
                ->isRealFloatingType();
    }

    return false;
}

class DependencyVisitor :
    public RecursiveASTVisitor<DependencyVisitor> {

public:
    std::set<std::string> dependencies;
    unsigned arithmeticOps = 0;
    unsigned literals = 0;

    bool VisitDeclRefExpr(DeclRefExpr *expr) {

        const ValueDecl *decl =
            expr->getDecl();

        if(!decl)
            return true;

        QualType type =
            decl->getType();

        if(isFloatingCarrier(type))
            dependencies.insert(
                decl->getNameAsString());

        return true;
    }

    bool VisitBinaryOperator(BinaryOperator *op) {

        if(op->isAdditiveOp()
            || op->isMultiplicativeOp()
            || op->isAssignmentOp())
            arithmeticOps++;

        return true;
    }

    bool VisitUnaryOperator(UnaryOperator *op) {

        if(op->getType()->isRealFloatingType())
            arithmeticOps++;

        return true;
    }

    bool VisitCallExpr(CallExpr *call) {

        if(call->getType()->isRealFloatingType())
            arithmeticOps += 2;

        return true;
    }

    bool VisitFloatingLiteral(FloatingLiteral *) {

        literals++;

        return true;
    }

    double estimateLocalError() const {

        return
            (static_cast<double>(arithmeticOps) * 1e-4)
            + (static_cast<double>(literals) * 1e-5);
    }
};

static void propagateBackward() {

    std::map<std::string, double>
    requirements;

    double initialBudget =
        std::max(
            OutputTolerance - returnExpressionError,
            OutputTolerance * 0.5);

    if(!returnDependencies.empty())
        initialBudget /=
            static_cast<double>(
                returnDependencies.size());

    for(const std::string &name : returnDependencies)
        requirements[name] = initialBudget;

    if(requirements.empty()) {

        for(const FlowNode &node : flowNodes)
            requirements[node.name] = OutputTolerance;
    }

    for(auto it = flowNodes.rbegin();
        it != flowNodes.rend();
        ++it) {

        FlowNode &node = *it;

        double required =
            OutputTolerance;

        auto found =
            requirements.find(node.name);

        if(found != requirements.end())
            required = found->second;

        node.requiredTolerance = required;
        node.assignedType =
            choosePrecision(
                node.localError,
                required);

        precisionTable[node.name] = {
            node.name,
            node.localError,
            node.assignedType
        };

        double childBudget =
            std::max(
                required - node.localError,
                required * 0.5);

        if(!node.dependencies.empty())
            childBudget /=
                static_cast<double>(
                    node.dependencies.size());

        for(const std::string &dep : node.dependencies) {

            auto depReq =
                requirements.find(dep);

            if(depReq == requirements.end())
                requirements[dep] = childBudget;
            else
                depReq->second =
                    std::min(
                        depReq->second,
                        childBudget);
        }
    }
}

static FlowNode *findFlowNode(
    const std::string &name) {

    for(FlowNode &node : flowNodes) {

        if(node.name == name)
            return &node;
    }

    return nullptr;
}

static void rewriteDemotions() {

    for(const FlowNode &node : flowNodes) {

        if(!node.rewriteAllowed)
            continue;

        std::string newType =
            precisionToString(
                node.assignedType);

        if(newType == "float")
            continue;

        Rewrite.ReplaceText(
            node.typeLoc,
            5,
            newType);

        std::cout
            << "\n[3] AST REWRITING\n";

        std::cout
            << "Variable "
            << node.name
            << " rewritten to "
            << newType
            << "\n";
    }
}

// ============================================
// DELIVERABLE 1
// AST ANALYSIS + DATA-FLOW COLLECTION
// ============================================
class AnalysisHandler :
    public MatchFinder::MatchCallback {

public:

    void run(
        const MatchFinder::MatchResult &Result) override {

        ASTContext *context =
            Result.Context;

        if(!context)
            return;

        if(const VarDecl *var =
            Result.Nodes.getNodeAs<VarDecl>("varDecl")) {

            if(!var || !var->hasInit())
                return;

            if(var->getType().getAsString()
                != "float")
                return;

            DependencyVisitor visitor;
            visitor.TraverseStmt(
                const_cast<Expr *>(var->getInit()));

            FlowNode node;
            node.name =
                var->getNameAsString();
            node.rhs =
                printExpr(
                    var->getInit(),
                    *context);
            node.typeLoc =
                var->getTypeSpecStartLoc();
            node.localError =
                visitor.estimateLocalError();
            node.dependencies =
                visitor.dependencies;

            flowNodes.push_back(node);

            addAstDotExpression(
                node.name + " = " + node.rhs,
                var->getInit(),
                *context);

            std::cout
                << "\n====================================\n";

            std::cout
                << "[1] AST DATA FLOW ANALYSIS\n";

            std::cout
                << "Variable : "
                << node.name
                << "\n";

            std::cout
                << "Flow     : "
                << node.name
                << " <- "
                << node.rhs
                << "\n";

            std::cout
                << "Depends  : ";

            if(node.dependencies.empty()) {

                std::cout
                    << "(source/literal)";
            } else {

                bool first = true;

                for(const std::string &dep :
                    node.dependencies) {

                    if(!first)
                        std::cout << ", ";

                    std::cout << dep;
                    first = false;
                }
            }

            std::cout
                << "\n";

            std::cout
                << "Local Error Estimate : "
                << node.localError
                << "\n";
        }

        if(const ParmVarDecl *param =
            Result.Nodes.getNodeAs<ParmVarDecl>("paramDecl")) {

            if(!param)
                return;

            if(param->getType().getAsString()
                != "float")
                return;

            FlowNode node;
            node.name =
                param->getNameAsString();
            node.rhs =
                "(function input)";
            node.typeLoc =
                param->getTypeSpecStartLoc();
            node.localError =
                0.0;
            node.rewriteAllowed =
                false;

            flowNodes.push_back(node);

            std::cout
                << "\n====================================\n";

            std::cout
                << "[1] AST DATA FLOW ANALYSIS\n";

            std::cout
                << "Input    : "
                << node.name
                << "\n";

            std::cout
                << "Flow     : "
                << node.name
                << " <- "
                << node.rhs
                << "\n";
        }

        if(const ReturnStmt *ret =
            Result.Nodes.getNodeAs<ReturnStmt>("ret")) {

            if(!ret->getRetValue())
                return;

            DependencyVisitor visitor;
            visitor.TraverseStmt(
                const_cast<Expr *>(ret->getRetValue()));

            returnDependencies.insert(
                visitor.dependencies.begin(),
                visitor.dependencies.end());

            returnExpressionError +=
                visitor.estimateLocalError();

            addAstDotExpression(
                "return " + printExpr(
                    ret->getRetValue(),
                    *context),
                ret->getRetValue(),
                *context);

            std::cout
                << "\n[RETURN FLOW]\n";

            std::cout
                << "return <- "
                << printExpr(
                    ret->getRetValue(),
                    *context)
                << "\n";

            std::cout
                << "Return Error Estimate : "
                << visitor.estimateLocalError()
                << "\n";
        }

        if(const BinaryOperator *assign =
            Result.Nodes.getNodeAs<BinaryOperator>("assign")) {

            const Expr *lhs =
                assign->getLHS()
                    ->IgnoreParenImpCasts();

            const DeclRefExpr *lhsRef =
                dyn_cast<DeclRefExpr>(lhs);

            if(!lhsRef)
                return;

            const ValueDecl *decl =
                lhsRef->getDecl();

            if(!decl
                || !isFloatingCarrier(
                    decl->getType()))
                return;

            std::string name =
                decl->getNameAsString();

            FlowNode *node =
                findFlowNode(name);

            if(!node)
                return;

            DependencyVisitor visitor;
            visitor.TraverseStmt(
                const_cast<Expr *>(
                    assign->getRHS()));

            node->dependencies.insert(
                visitor.dependencies.begin(),
                visitor.dependencies.end());

            node->localError +=
                std::max(
                    visitor.estimateLocalError(),
                    1e-4);

            addAstDotExpression(
                name + " = " + printExpr(
                    assign->getRHS(),
                    *context),
                assign->getRHS(),
                *context);

            std::cout
                << "\n[ASSIGNMENT FLOW]\n";

            std::cout
                << name
                << " <- "
                << printExpr(
                    assign->getRHS(),
                    *context)
                << "\n";
        }
    }
};

// ============================================
// FRONTEND ACTION
// ============================================
class MyFrontendAction :
    public ASTFrontendAction {

public:

    MatchFinder finder;

    AnalysisHandler analysisHandler;

    std::unique_ptr<ASTConsumer>
    CreateASTConsumer(
        CompilerInstance &CI,
        StringRef file) override {

        Rewrite.setSourceMgr(
            CI.getSourceManager(),
            CI.getLangOpts());

        finder.addMatcher(

            varDecl(
                hasInitializer(expr()),
                unless(parmVarDecl()),
                isExpansionInMainFile()
            ).bind("varDecl"),

            &analysisHandler
        );

        finder.addMatcher(

            parmVarDecl(
                hasType(asString("float")),
                isExpansionInMainFile()
            ).bind("paramDecl"),

            &analysisHandler
        );

        finder.addMatcher(

            binaryOperator(
                isAssignmentOperator(),
                isExpansionInMainFile()
            ).bind("assign"),

            &analysisHandler
        );

        finder.addMatcher(

            returnStmt(
                isExpansionInMainFile()
            ).bind("ret"),

            &analysisHandler
        );

        return finder.newASTConsumer();
    }

    void EndSourceFileAction()
        override {

        std::cout
            << "\n[2] BACKWARD PRECISION PROPAGATION\n";

        std::cout
            << "Output Tolerance : "
            << OutputTolerance
            << "\n";

        std::cout
            << "Return Expression Error : "
            << returnExpressionError
            << "\n";

        propagateBackward();

        for(const FlowNode &node : flowNodes) {

            std::cout
                << "Variable "
                << node.name
                << " requires <= "
                << node.requiredTolerance
                << ", local error "
                << node.localError
                << ", chosen "
                << precisionToString(
                    node.assignedType)
                << "\n";
        }

        rewriteDemotions();

        writeAstDotFile();

        llvm::SmallString<256>
        outPath(OutputFile);

        llvm::sys::fs::create_directories(
            llvm::sys::path::parent_path(outPath));

        std::error_code EC;

        llvm::raw_fd_ostream out(
            OutputFile,
            EC);

        if(EC) {

            llvm::errs()
                << "Failed to open output file "
                << OutputFile
                << ": "
                << EC.message()
                << "\n";

            return;
        }

        Rewrite.getEditBuffer(
            Rewrite.getSourceMgr()
            .getMainFileID()
        ).write(out);

        out.close();

        std::cout
            << "\n====================================\n";

        std::cout
            << "[FINAL REWRITTEN CODE]\n";

        std::cout
            << "Saved to:\n";

        std::cout
            << OutputFile
            << "\n";

        std::cout
            << "\n====================================\n";

        Rewrite.getEditBuffer(
            Rewrite.getSourceMgr()
            .getMainFileID()
        ).write(llvm::outs());

        std::cout
            << "\n====================================\n";
    }
};

// ============================================
// MAIN
// ============================================
int main(
    int argc,
    const char **argv) {

    auto ExpectedParser =
        CommonOptionsParser::create(
            argc,
            argv,
            Category);

    if(!ExpectedParser) {

        llvm::errs()
            << ExpectedParser.takeError();

        return 1;
    }

    CommonOptionsParser &OptionsParser =
        ExpectedParser.get();

    ClangTool Tool(
        OptionsParser.getCompilations(),
        OptionsParser.getSourcePathList());

    std::cout
        << "\n====================================\n";

    std::cout
        << "PRECISION DEMOTION FRAMEWORK\n";

    std::cout
        << "====================================\n";

    int result =
        Tool.run(
            newFrontendActionFactory
            <MyFrontendAction>().get());

    std::cout
        << "\n====================================\n";

    std::cout
        << "ANALYSIS COMPLETE\n";

    std::cout
        << "====================================\n";

    return result;
}
