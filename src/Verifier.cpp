#include <iostream>
#include <fstream>
#include <chrono>
#include <cstdlib>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

using namespace std;
using namespace std::chrono;

// ==========================================
// Run command and measure execution time
// ==========================================
struct CommandResult {
    double elapsedMs;
    int exitCode;
};

CommandResult runCommand(const string &cmd) {

    auto start =
        high_resolution_clock::now();

    int status =
        system(cmd.c_str());

    auto end =
        high_resolution_clock::now();

    int exitCode =
        status;

    if(status != -1 && WIFEXITED(status))
        exitCode =
            WEXITSTATUS(status);

    return {
        duration<double, milli>(
            end - start).count(),
        exitCode
    };
}

// ==========================================
// Read numeric output from file
// ==========================================
bool readOutput(
    const string &file,
    double &value) {

    ifstream in(file);

    if(!in)
        return false;

    in >> value;

    return !in.fail();
}

string shellQuote(const string &value) {

    string quoted = "'";

    for(char c : value) {

        if(c == '\'')
            quoted += "'\\''";
        else
            quoted += c;
    }

    quoted += "'";

    return quoted;
}

string sanitizeName(string value) {

    for(char &c : value) {

        if(!(isalnum(static_cast<unsigned char>(c))
            || c == '_'
            || c == '-'))
            c = '_';
    }

    return value;
}

string displayInputName(const string &value) {

    size_t slash =
        value.find_last_of("/\\");

    if(slash == string::npos)
        return value;

    return value.substr(slash + 1);
}

bool containsToken(
    const string &text,
    const string &token) {

    size_t pos = text.find(token);

    while(pos != string::npos) {

        bool leftOk =
            pos == 0
            || !(isalnum(static_cast<unsigned char>(
                    text[pos - 1]))
                || text[pos - 1] == '_');

        size_t right =
            pos + token.size();

        bool rightOk =
            right >= text.size()
            || !(isalnum(static_cast<unsigned char>(
                    text[right]))
                || text[right] == '_');

        if(leftOk && rightOk)
            return true;

        pos =
            text.find(token, pos + token.size());
    }

    return false;
}

string precisionSummaryForFile(const string &file) {

    ifstream in(file);

    if(!in)
        return "Unknown";

    stringstream buffer;
    buffer << in.rdbuf();
    string text = buffer.str();

    bool hasFp16 =
        containsToken(text, "__fp16");

    bool hasBf16 =
        containsToken(text, "__bf16");

    bool hasFloat =
        containsToken(text, "float");

    bool hasDouble =
        containsToken(text, "double");

    int reducedCount =
        (hasFp16 ? 1 : 0)
        + (hasBf16 ? 1 : 0);

    if(reducedCount > 1)
        return "Mixed";

    if(hasFp16)
        return "FP16";

    if(hasBf16)
        return "BF16";

    int sourceCount =
        (hasFloat ? 1 : 0)
        + (hasDouble ? 1 : 0);

    if(sourceCount > 1)
        return "Mixed";

    if(hasDouble)
        return "FP64";

    if(hasFloat)
        return "FP32";

    return "None";
}

string csvEscape(const string &value) {

    bool needsQuotes = false;
    string escaped;

    for(char c : value) {

        if(c == '"' || c == ',' || c == '\n' || c == '\r')
            needsQuotes = true;

        if(c == '"')
            escaped += "\"\"";
        else
            escaped += c;
    }

    if(!needsQuotes)
        return escaped;

    return "\"" + escaped + "\"";
}

int decimalPlacesForTolerance(
    const string &text) {

    size_t dot =
        text.find('.');

    if(dot == string::npos)
        return 0;

    size_t end =
        text.find_first_of("eE", dot);

    if(end == string::npos)
        end = text.size();

    return static_cast<int>(
        end - dot - 1);
}

string fixedDecimal(
    double value,
    int decimals) {

    ostringstream out;

    out
        << fixed
        << setprecision(decimals)
        << value;

    return out.str();
}

string jsonEscape(const string &value) {

    ostringstream out;

    for(char c : value) {

        switch(c) {
            case '"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            case '\b':
                out << "\\b";
                break;
            case '\f':
                out << "\\f";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if(static_cast<unsigned char>(c) < 0x20) {
                    out
                        << "\\u"
                        << hex
                        << setw(4)
                        << setfill('0')
                        << static_cast<int>(
                            static_cast<unsigned char>(c))
                        << dec
                        << setfill(' ');
                } else {
                    out << c;
                }
        }
    }

    return out.str();
}

bool startsWith(
    const string &value,
    const string &prefix) {

    return value.rfind(prefix, 0) == 0;
}

// ==========================================
// MAIN
// ==========================================
int main(int argc, char **argv) {

    if(argc < 2) {

        cout << "Usage:\n";
        cout << "./verify <kernel.cpp> [tolerance]\n";

        return 1;
    }

    string inputFile = argv[1];

    string tempDir =
        "../temp/verify_"
        + sanitizeName(inputFile)
        + "_"
        + to_string(getpid());

    string originalCpp =
        tempDir + "/original.cpp";

    string demotedCpp =
        tempDir + "/demoted.cpp";

    string originalOut =
        tempDir + "/original.out";

    string demotedOut =
        tempDir + "/demoted.out";

    string originalResult =
        tempDir + "/original_result.txt";

    string demotedResult =
        tempDir + "/demoted_result.txt";

    string toleranceText = "0.01";

    double tolerance = 1e-2;

    int optionStart = 2;

    if(argc >= 3 && !startsWith(argv[2], "--")) {

        toleranceText =
            argv[2];

        tolerance =
            atof(argv[2]);

        if(tolerance <0) {

            cout << "Tolerance must be positive\n";

            return 1;
        }

        optionStart = 3;
    }

    string jsonOutputFile = "";

    string astDotFile = "";

    for(int i = optionStart; i < argc; ++i) {

        string arg = argv[i];

        if(startsWith(arg, "--json=")) {
            jsonOutputFile =
                arg.substr(
                    string("--json=").size());
        } else if(startsWith(arg, "--ast-dot=")) {
            astDotFile =
                arg.substr(
                    string("--ast-dot=").size());
        } else {
            cout << "Unknown option: "
                 << arg
                 << "\n";

            return 1;
        }
    }

    int displayDecimals =
        decimalPlacesForTolerance(
            toleranceText);

    // ======================================
    // Generate demoted version
    // ======================================

    cout << "\n[1] Running Clang Tool\n";

    ostringstream toolCmd;

    toolCmd
        << "./tool "
        << shellQuote(inputFile)
        << " --tolerance="
        << tolerance
        << " --output="
        << shellQuote(demotedCpp);

    if(!astDotFile.empty()) {
        toolCmd
            << " --ast-dot="
            << shellQuote(astDotFile);
    }

    toolCmd
        << " -- -std=c++17";

    string mkdirCmd =
        "mkdir -p " + shellQuote(tempDir);

    CommandResult toolRun =
        runCommand(mkdirCmd);

    if(toolRun.exitCode != 0) {

        cout << "Could not create temp directory, exit code "
             << toolRun.exitCode
             << "\n";

        return 1;
    }

    toolRun =
        runCommand(
            toolCmd.str());

    if(toolRun.exitCode != 0) {

        cout << "Clang tool failed with exit code "
             << toolRun.exitCode
             << "\n";

        return 1;
    }

    // ======================================
    // Copy original
    // ======================================

    string copyCmd =
        "cp "
        + shellQuote(inputFile)
        + " "
        + shellQuote(originalCpp);

    CommandResult copyRun =
        runCommand(copyCmd);

    if(copyRun.exitCode != 0) {

        cout << "Failed to copy original input\n";

        return 1;
    }

    // ======================================
    // Compile Original
    // ======================================

    cout << "[2] Compiling Original\n";

    string compileOrig =
        "clang++ "
        + shellQuote(originalCpp)
        + " -o "
        + shellQuote(originalOut);

    CommandResult origCompile =
        runCommand(compileOrig);

    if(origCompile.exitCode != 0) {

        cout << "Original compilation failed\n";

        return 1;
    }

    // ======================================
    // Compile Demoted
    // ======================================

    cout << "[3] Compiling Demoted\n";

    string compileDemo =
        "clang++ "
        + shellQuote(demotedCpp)
        + " -o "
        + shellQuote(demotedOut);

    CommandResult demoCompile =
        runCommand(compileDemo);

    if(demoCompile.exitCode != 0) {

        cout << "Demoted compilation failed\n";

        return 1;
    }

    // ======================================
    // Execute Original
    // ======================================

    cout << "[4] Running Original\n";

    CommandResult origRuntime =
        runCommand(
            shellQuote(originalOut)
            + " > "
            + shellQuote(originalResult));

    if(origRuntime.exitCode != 0) {

        cout << "Original execution failed\n";

        return 1;
    }

    // ======================================
    // Execute Demoted
    // ======================================

    cout << "[5] Running Demoted\n";

    CommandResult demoRuntime =
        runCommand(
            shellQuote(demotedOut)
            + " > "
            + shellQuote(demotedResult));

    if(demoRuntime.exitCode != 0) {

        cout << "Demoted execution failed\n";

        return 1;
    }

    // ======================================
    // Read outputs
    // ======================================

    double origOutput = 0;
    double demoOutput = 0;

    if(!readOutput(
        originalResult,
        origOutput)) {

        cout << "Could not read numeric original output\n";

        return 1;
    }

    if(!readOutput(
        demotedResult,
        demoOutput)) {

        cout << "Could not read numeric demoted output\n";

        return 1;
    }

    // ======================================
    // Accuracy loss
    // ======================================

    double error =
        fabs(origOutput - demoOutput);

    // ======================================
    // Performance gain and speedup
    // ======================================

    double performanceGainPercent =
        ((origRuntime.elapsedMs - demoRuntime.elapsedMs)
        / origRuntime.elapsedMs)
        * 100.0;

    double speedup =
        origRuntime.elapsedMs
        / demoRuntime.elapsedMs;

    constexpr double comparisonEpsilon = 1e-12;

    bool passed =
        error <= tolerance + comparisonEpsilon;

    // ======================================
    // Final Report
    // ======================================

    cout << "\n====================================\n";

    cout << "Verification Report\n";

    cout << "====================================\n";

    cout << "Original Output: "
         << origOutput << "\n";

    cout << "Demoted Output: "
         << demoOutput << "\n";

    cout << "Accuracy Loss: "
         << fixedDecimal(
                error,
                displayDecimals)
         << "\n";

    cout << "Tolerance: "
         << fixedDecimal(
                tolerance,
                displayDecimals)
         << "\n";

    cout << "Verification: "
         << (passed ? "PASS" : "FAIL")
         << "\n";

    cout << "Original Runtime: "
         << origRuntime.elapsedMs
         << " ms\n";

    cout << "Demoted Runtime: "
         << demoRuntime.elapsedMs
         << " ms\n";

    cout << "Performance Gain (%): "
         << performanceGainPercent
         << "%\n";

    cout << "Speedup: "
         << speedup
         << "x\n";

    cout << "\nOriginal Compile Time: "
         << origCompile.elapsedMs
         << " ms\n";

    cout << "Demoted Compile Time: "
         << demoCompile.elapsedMs
         << " ms\n";

    // ======================================
    // CSV Export
    // ======================================

    system("mkdir -p ../results");

    const string csvPath =
        "../results/evaluation.csv";

    const string csvHeader =
        "Kernel/File,Original Type,Demoted Type,Accuracy Loss,Speedup,Performance Gain (%)";

    bool writeHeader = true;

    {
        ifstream existing(csvPath);
        string firstLine;

        if(existing && getline(existing, firstLine))
            writeHeader =
                firstLine != csvHeader;
    }

    ofstream csv(
        csvPath,
        writeHeader ? ios::trunc : ios::app);

    csv << setprecision(17);

    if(writeHeader)
        csv << csvHeader << "\n";

    csv
        << csvEscape(displayInputName(inputFile)) << ","
        << precisionSummaryForFile(originalCpp) << ","
        << precisionSummaryForFile(demotedCpp) << ","
        << error << ","
        << speedup << ","
        << performanceGainPercent
        << "\n";

    cout << "\nSaved to:\n";
    cout << "../results/evaluation.csv\n";

    if(!jsonOutputFile.empty()) {

        ofstream json(jsonOutputFile);

        if(!json) {
            cout << "Could not write JSON report to "
                 << jsonOutputFile
                 << "\n";

            return 1;
        }

        json << setprecision(17);

        json << "{\n";
        json << "  \"inputFile\": \""
             << jsonEscape(inputFile)
             << "\",\n";
        json << "  \"tolerance\": "
             << tolerance
             << ",\n";
        json << "  \"originalOutput\": "
             << origOutput
             << ",\n";
        json << "  \"demotedOutput\": "
             << demoOutput
             << ",\n";
        json << "  \"accuracyLoss\": "
             << error
             << ",\n";
        json << "  \"verification\": \""
             << (passed ? "PASS" : "FAIL")
             << "\",\n";
        json << "  \"passed\": "
             << (passed ? "true" : "false")
             << ",\n";
        json << "  \"originalRuntimeMs\": "
             << origRuntime.elapsedMs
             << ",\n";
        json << "  \"demotedRuntimeMs\": "
             << demoRuntime.elapsedMs
             << ",\n";
        json << "  \"performanceGainPercent\": "
             << performanceGainPercent
             << ",\n";
        json << "  \"speedup\": "
             << speedup
             << ",\n";
        json << "  \"originalCompileTimeMs\": "
             << origCompile.elapsedMs
             << ",\n";
        json << "  \"demotedCompileTimeMs\": "
             << demoCompile.elapsedMs
             << ",\n";
        json << "  \"paths\": {\n";
        json << "    \"tempDir\": \""
             << jsonEscape(tempDir)
             << "\",\n";
        json << "    \"originalSource\": \""
             << jsonEscape(originalCpp)
             << "\",\n";
        json << "    \"demotedSource\": \""
             << jsonEscape(demotedCpp)
             << "\",\n";
        json << "    \"originalResult\": \""
             << jsonEscape(originalResult)
             << "\",\n";
        json << "    \"demotedResult\": \""
             << jsonEscape(demotedResult)
             << "\",\n";
        json << "    \"csv\": \"../results/evaluation.csv\"";

        if(!astDotFile.empty()) {
            json << ",\n";
            json << "    \"astDot\": \""
                 << jsonEscape(astDotFile)
                 << "\"\n";
        } else {
            json << "\n";
        }

        json << "  }\n";
        json << "}\n";

        cout << "JSON report saved to:\n";
        cout << jsonOutputFile << "\n";
    }

    return passed ? 0 : 1;
}
