#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <boost/circular_buffer.hpp>

namespace {
int checks = 0;
void require(bool ok, const char *message)
{
    ++checks;
    if (!ok) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
}

int main()
{
    boost::circular_buffer<int> values(3);
    require(values.capacity() == 3 && values.empty(), "reserved capacity starts empty");
    values.push_back(1);
    values.push_back(2);
    require(values.size() == 2 && values[0] == 1 && values[1] == 2,
            "append below capacity preserves oldest");
    values.push_back(3);
    require(values.full() && values.front() == 1 && values.back() == 3,
            "filling last slot does not overwrite");
    values.push_back(4);
    require(values.size() == 3 && values[0] == 2 && values[1] == 3 && values[2] == 4,
            "first overflow replaces only oldest");
    for (int i = 5; i <= 100; ++i) {
        values.push_back(i);
        require(values.capacity() == 3 && values.size() == 3
                && values[0] == i - 2 && values[1] == i - 1 && values[2] == i,
                "repeated wrap keeps newest values in logical order");
    }
    values.pop_front();
    values.push_back(101);
    require(values[0] == 99 && values[1] == 100 && values[2] == 101,
            "append after pop fills vacancy before overwriting");
    std::array<int, 3> copy{};
    std::copy_n(values.begin(), copy.size(), copy.begin());
    require(copy == std::array{99, 100, 101}, "iterators follow logical order across wrap");
    values.erase_begin(2);
    require(values.size() == 1 && values.front() == 101, "erase consumed prefix");
    values.erase_begin(1);
    require(values.empty() && values.capacity() == 3, "drain keeps reserved capacity");
    values.push_back(102);
    require(values.size() == 1 && values.front() == 102, "reuse drained buffer");

    // 与风廓线相同的只读共享份额，覆盖时释放旧份额，未满时保留。
    boost::circular_buffer<std::shared_ptr<const int>> frames(2);
    auto first = std::make_shared<const int>(1);
    std::weak_ptr<const int> observed = first;
    frames.push_back(std::move(first));
    frames.push_back(std::make_shared<const int>(2));
    require(!observed.expired() && *frames.front() == 1, "full buffer retains first frame");
    frames.push_back(std::make_shared<const int>(3));
    require(observed.expired() && *frames.front() == 2 && *frames.back() == 3,
            "overflow releases only overwritten shared frame");
    frames.clear();
    require(frames.empty() && frames.capacity() == 2, "clear preserves capacity");

    boost::circular_buffer<char> single(1);
    single.push_back('a');
    single.push_back('b');
    require(single.size() == 1 && single.front() == 'b', "one slot overwrites correctly");
    std::cout << checks << " checks passed\n";
}
