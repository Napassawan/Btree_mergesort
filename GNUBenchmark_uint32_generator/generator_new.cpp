#include <string.h>

#include <iostream>
#include <string>
#include <memory>
#include <cfloat>
#include <ctime>

#include <variant>
#include <vector>
#include <stack>
#include <deque>

#include <random>
#include <functional>
#include <execution>
#include <algorithm>

#include <omp.h>

using std::string;
using std::variant;
using std::vector;

using std::unique_ptr;

// ------------------------------------------------------------------------------

constexpr const float Opt_FewUnique_UniquePercentage = 0.05;	// 5%

constexpr const float Opt_NSorted_SwapPercentage = 0.05;		// 5%

// ------------------------------------------------------------------------------

enum class DataType {
	i32,
	u32,
	i64,
	u64,
	f64,
	Invalid,
};
DataType GetDataTypeFromString(char* name);

enum class DataArrangeType {
	Random,
	Reversed,
	FewUnique,
	NearlySorted,
	Invalid,
};
DataArrangeType GetDataArrangeTypeFromString(char* name);

// ------------------------------------------------------------------------------

void GenerateDataFromType(size_t count, DataType type, DataArrangeType arrangement);
template<typename T> void GenerateDataFromArrangement(size_t count, DataArrangeType arrangement);

template<typename T> class DataGenerator {
public:
	DataGenerator();
	T operator()();
};
template<typename T> void GenerateRandom(vector<T>& res);
template<typename T> void GenerateReversed(vector<T>& res);
template<typename T> void GenerateFewUnique(vector<T>& res);
template<typename T> void GenerateNearlySorted(vector<T>& res);

// Global rand
std::mt19937_64 mt64((uint64_t)time(nullptr) * 2);

// ------------------------------------------------------------------------------

void PrintHelp() {
	printf("Arguments: N [, DataType [, Arrangement]]\n");
	printf("    DataType can be:    i32, u32, i64, u64, f64\n");
	printf("    Arrangement can be: random, reversed, fewunique, nsorted\n");
}
int main(int argc, char** argv) {
	// Args: N [, DataType [, Arrangement]]
	
	if (argc < 2) {
		PrintHelp();
		return 0;
	}
	
	uint64_t countData = std::strtoull(argv[1], nullptr, 10);
	DataType typeDataGenerate = DataType::f64;
	DataArrangeType typeDataArrangement = DataArrangeType::Random;
	
	if (argc > 2) {
		typeDataGenerate = GetDataTypeFromString(argv[2]);
	}
	if (argc > 3) {
		typeDataArrangement = GetDataArrangeTypeFromString(argv[3]);
	}
	
	if (typeDataGenerate == DataType::Invalid || typeDataArrangement == DataArrangeType::Invalid) {
		PrintHelp();
		return 0;
	}
	
	GenerateDataFromType(countData, typeDataGenerate, typeDataArrangement);
	
	//printf("Done");
	return 0;
}

void GenerateDataFromType(size_t count, DataType type, DataArrangeType arrangement) {
	switch (type) {
	case DataType::i32:
		GenerateDataFromArrangement<int32_t>(count, arrangement);
		break;
	case DataType::u32:
		GenerateDataFromArrangement<uint32_t>(count, arrangement);
		break;
	case DataType::i64:
		GenerateDataFromArrangement<int64_t>(count, arrangement);
		break;
	case DataType::u64:
		GenerateDataFromArrangement<uint64_t>(count, arrangement);
		break;
	case DataType::f64:
		GenerateDataFromArrangement<double_t>(count, arrangement);
		break;
	default: break;
	}
}

// ------------------------------------------------------------------------------

DataType GetDataTypeFromString(char* name) {
#define CHECK(_chk, _type) if (strcmpi(name, _chk) == 0) return _type

	CHECK("i32", DataType::i32);
	else CHECK("u32", DataType::u32);
	else CHECK("i64", DataType::i64);
	else CHECK("u64", DataType::u64);
	else CHECK("f64", DataType::f64);
	else CHECK("double", DataType::f64);

	return DataType::Invalid;

#undef CHECK
}
DataArrangeType GetDataArrangeTypeFromString(char* name) {
#define CHECK(_chk, _type) if (strcmpi(name, _chk) == 0) return _type

	CHECK("random", DataArrangeType::Random);
	else CHECK("reverse", DataArrangeType::Reversed);
	else CHECK("reversed", DataArrangeType::Reversed);
	else CHECK("fewunique", DataArrangeType::FewUnique);
	else CHECK("nsort", DataArrangeType::NearlySorted);
	else CHECK("nsorted", DataArrangeType::NearlySorted);
	
	return DataArrangeType::Invalid;

#undef CHECK
}

// ------------------------------------------------------------------------------

template<>
class DataGenerator<int32_t> {
	std::mt19937 mt;
public:
	DataGenerator() : mt((uint64_t)time(nullptr)) {}
	int32_t operator()() {
		return mt();
	}
};
template<>
class DataGenerator<uint32_t> {
	std::mt19937 mt;
public:
	DataGenerator() : mt((uint64_t)time(nullptr)) {}
	uint32_t operator()() {
		return mt();
	}
};
template<>
class DataGenerator<int64_t> {
	std::mt19937_64 mt;
public:
	DataGenerator() : mt((uint64_t)time(nullptr)) {}
	int64_t operator()() {
		return mt();
	}
};
template<>
class DataGenerator<uint64_t> {
	std::mt19937_64 mt;
public:
	DataGenerator() : mt((uint64_t)time(nullptr)) {}
	uint64_t operator()() {
		return mt();
	}
};
template<>
class DataGenerator<double_t> {
	std::mt19937_64 mt;
	std::uniform_real_distribution<> dis;
public:
	DataGenerator() : mt((uint64_t)time(nullptr)),
		dis(-10000.0, 10000.0) {}
	double_t operator()() {
		return dis(mt);
	}
};

template<typename T> void GenerateDataFromArrangement(size_t count, DataArrangeType arrangement) {
	vector<T> data(count);
	
	switch (arrangement) {
	case DataArrangeType::Random:
		GenerateRandom<T>(data);
		break;
	case DataArrangeType::Reversed:
		GenerateReversed<T>(data);
		break;
	case DataArrangeType::FewUnique:
		GenerateFewUnique<T>(data);
		break;
	case DataArrangeType::NearlySorted:
		GenerateNearlySorted<T>(data);
		break;
	default: break;
	}
	
	for (T& i : data) {
		printf("%s ", std::to_string(i).c_str());
	}
}

template<typename T> void GenerateRandom(vector<T>& res) {
	DataGenerator<T> generator;
	
	// Generate data
	for (size_t i = 0; i < res.size(); ++i) {
		res[i] = generator();
	}
};
template<typename T> void GenerateReversed(vector<T>& res) {
	DataGenerator<T> generator;
	
	// Generate data
	for (size_t i = 0; i < res.size(); ++i) {
		res[i] = generator();
	}
	
	// Sort descending
	std::sort(std::execution::par, res.begin(), res.end(), std::greater<T>());
};
template<typename T> void GenerateFewUnique(vector<T>& res) {
	DataGenerator<T> generator;
	
	// Guarantee at least 2 uniques
	size_t countUnique = std::max<size_t>(
		2, res.size() * Opt_FewUnique_UniquePercentage);
	
	// Generate array where len(tmp) < len(res)
	vector<T> tmp(countUnique);
	for (size_t i = 0; i < countUnique; ++i) {
		tmp[i] = generator();
	}
	
	// Randomly choose from tmp to fill into output array
	for (size_t i = 0; i < res.size(); ++i) {
		res[i] = tmp[mt64() % countUnique];
	}
};
template<typename T> void GenerateNearlySorted(vector<T>& res) {
	DataGenerator<T> generator;
	
	// Guarantee at least 1 swap
	size_t countSwap = std::max<size_t>(
		1, res.size() * Opt_NSorted_SwapPercentage);
	
	// Generate data
	for (size_t i = 0; i < res.size(); ++i) {
		res[i] = generator();
	}
	
	// Sort ascending
	std::sort(std::execution::par, res.begin(), res.end(), std::less<T>());
	
	// Then randomly swap some elements
	for (size_t i = 0; i < countSwap; ++i) {
		size_t swapA = mt64() % res.size();
		size_t swapB = mt64() % res.size();
		std::swap(res[swapA], res[swapB]);
	}
};
