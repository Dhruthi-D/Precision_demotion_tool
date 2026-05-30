#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
using namespace std;

int main() {
    vector<pair<pair<int,int>, int>> data = {
        {{1,2},0}, {{2,3},0}, {{6,5},1}, {{7,7},1}
    };

    int x = 5, y = 5, k = 3;
    vector<pair<double,int>> dist;

    for (auto p : data) {
        double d = sqrt(pow(p.first.first - x, 2) +
                        pow(p.first.second - y, 2));
        dist.push_back({d, p.second});
    }

    sort(dist.begin(), dist.end());

    int cnt0 = 0, cnt1 = 0;
    for (int i = 0; i < k; i++) {
        if (dist[i].second == 0) cnt0++;
        else cnt1++;
    }

    cout << "Predicted class: "
         << (cnt1 > cnt0 ? 1 : 0);

    return 0;
}