#include "exclusive_output.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
struct Pick { std::string order; long long location; long long units; };

bool formula_like(const std::string& id) {
    const auto first = id.find_first_not_of(" \t\f\v\r\n");
    return first != std::string::npos && std::string("=+-@").find(id[first]) != std::string::npos;
}

long long number(const std::string& value, const std::string& field) {
    if (value.empty()) throw std::runtime_error("empty " + field);
    std::size_t end = 0;
    long long n;
    try { n = std::stoll(value, &end); }
    catch (const std::exception&) { throw std::runtime_error("invalid " + field + ": " + value); }
    if (end != value.size() || n < 0 || n > 1000000000LL)
        throw std::runtime_error("invalid " + field + ": " + value);
    return n;
}

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= line.size(); ++i) {
        if (i == line.size() || line[i] == ',') {
            fields.push_back(line.substr(start, i - start));
            start = i + 1;
        }
    }
    return fields;
}

void run(const fs::path& input, long long capacity, const fs::path& output) {
    if (capacity < 1 || capacity > 1000000000LL) throw std::runtime_error("capacity must be 1..1000000000");
    std::ifstream in(input);
    if (!in) throw std::runtime_error("cannot open input file");
    std::string line;
    if (!std::getline(in, line)) throw std::runtime_error("empty input file");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != "order_id,location,units") throw std::runtime_error("expected header order_id,location,units");
    std::vector<Pick> picks;
    std::set<std::string> ids;
    long long baseline = 0;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto row = split(line);
        if (row.size() != 3 || row[0].empty() || row[0].find_first_of("\"|\r\n") != std::string::npos)
            throw std::runtime_error("invalid order row");
        if (formula_like(row[0])) throw std::runtime_error("formula-like order_id is not allowed");
        if (!ids.insert(row[0]).second) throw std::runtime_error("duplicate order_id: " + row[0]);
        const long long location = number(row[1], "location");
        const long long units = number(row[2], "units");
        if (units == 0) throw std::runtime_error("units must be positive");
        if (units > capacity) throw std::runtime_error("pick exceeds cart capacity");
        picks.push_back({row[0], location, units});
        if (picks.size() > 100000) throw std::runtime_error("too many orders");
        baseline += 2 * location;
    }
    if (!in.eof()) throw std::runtime_error("error reading input");
    if (picks.empty()) throw std::runtime_error("empty order list");
    std::sort(picks.begin(), picks.end(), [](const Pick& a, const Pick& b) {
        return a.location != b.location ? a.location < b.location : a.order < b.order;
    });
    std::vector<std::string> rows;
    long long total = 0, load = 0, farthest = 0;
    std::string orders;
    auto finish = [&]() {
        if (orders.empty()) return;
        const long long distance = 2 * farthest;
        rows.push_back(std::to_string(rows.size() + 1) + "," + orders + "," + std::to_string(load) + "," + std::to_string(distance));
        total += distance;
        orders.clear(); load = 0; farthest = 0;
    };
    for (const Pick& pick : picks) {
        if (pick.units > capacity - load) finish();
        if (!orders.empty()) orders += "|";
        orders += pick.order;
        load += pick.units;
        farthest = pick.location;
    }
    finish();
    ExclusiveOutput file(output);
    std::ostream& out = file.stream();
    out << "batch_id,orders,units,walking_distance\n";
    for (const auto& row : rows) out << row << '\n';
    file.publish();
    std::cout << "batches=" << rows.size() << " baseline_distance=" << baseline << " batched_distance=" << total << '\n';
}

int main(int argc, char** argv) {
    try {
        if (argc != 4) throw std::runtime_error("usage: batch <orders.csv> <capacity> <batches.csv>");
        run(argv[1], number(argv[2], "capacity"), argv[3]);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
