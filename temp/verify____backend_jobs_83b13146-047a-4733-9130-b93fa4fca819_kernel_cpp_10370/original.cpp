#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <map>

using namespace std;

// Data point structure
struct Point {
    vector<double> features;
    int label;
};

// Calculate Euclidean distance
double euclideanDistance(const vector<double>& a, const vector<double>& b) {
    double sum = 0.0;
    for (size_t i = 0; i < a.size(); i++) {
        sum += pow(a[i] - b[i], 2);
    }
    return sqrt(sum);
}

// KNN classifier
int knn(const vector<Point>& dataset, const vector<double>& query, int k) {
    vector<pair<double, int>> distances;

    // Compute distance to all points
    for (const auto& point : dataset) {
        double dist = euclideanDistance(point.features, query);
        distances.push_back({dist, point.label});
    }

    // Sort by distance
    sort(distances.begin(), distances.end());

    // Count labels among k nearest neighbors
    map<int, int> labelCount;
    for (int i = 0; i < k; i++) {
        labelCount[distances[i].second]++;
    }

    // Find majority label
    int predictedLabel = -1;
    int maxCount = 0;

    for (auto& entry : labelCount) {
        if (entry.second > maxCount) {
            maxCount = entry.second;
            predictedLabel = entry.first;
        }
    }

    return predictedLabel;
}

int main() {
    // Training dataset
    vector<Point> dataset = {
        {{1.0, 2.0}, 0},
        {{2.0, 3.0}, 0},
        {{3.0, 3.0}, 0},
        {{6.0, 5.0}, 1},
        {{7.0, 7.0}, 1},
        {{8.0, 6.0}, 1}
    };

    vector<double> query = {5.0, 5.0};
    int k = 3;

    int prediction = knn(dataset, query, k);

    cout << "Predicted Class: " << prediction << endl;

    return 0;
}