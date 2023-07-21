#ifndef __INTERVAL_TREE_H
#define __INTERVAL_TREE_H

#include <vector>
#include <algorithm>
#include <iostream>
#include <memory>
#include <cassert>
#include <limits>

#ifdef USE_INTERVAL_TREE_NAMESPACE
namespace interval_tree
{
#endif
    template<class Scalar, typename Value>
    class Interval
    {
    public:
        Scalar start;
        Scalar stop;
        Value value;
        Interval(const Scalar& s, const Scalar& e, const Value& v) :
            start(std::min(s, e)), stop(std::max(s, e)), value(v)
        {
        }

        Interval()
        {
        }

        constexpr bool operator==(const Interval& other) const noexcept
        {
            return start == other.start &&
                   stop == other.stop &&
                   value == other.value;
        }

        constexpr bool operator!=(const Interval& other) const noexcept
        {
            return !(*this == other);
        }
    };

    template<class Scalar, typename Value>
    Value intervalStart(const Interval<Scalar, Value>& i)
    {
        return i.start;
    }

    template<class Scalar, typename Value>
    Value intervalStop(const Interval<Scalar, Value>& i)
    {
        return i.stop;
    }

    template<class Scalar, typename Value>
    std::ostream& operator<<(std::ostream& out, const Interval<Scalar, Value>& i)
    {
        out << "Interval(" << i.start << ", " << i.stop << "): " << i.value;
        return out;
    }

    template<class Scalar, class Value>
    class IntervalTree
    {
    public:
        typedef Interval<Scalar, Value> interval;
        typedef std::vector<interval> interval_vector;

        struct IntervalStartCmp
        {
            bool operator()(const interval& a, const interval& b)
            {
                return a.start < b.start;
            }
        };

        struct IntervalStopCmp
        {
            bool operator()(const interval& a, const interval& b)
            {
                return a.stop < b.stop;
            }
        };

        IntervalTree() :
            left(nullptr), right(nullptr), center()
        {
        }

        ~IntervalTree() = default;

        std::unique_ptr<IntervalTree> clone() const
        {
            return std::unique_ptr<IntervalTree>(new IntervalTree(*this));
        }

        IntervalTree(const IntervalTree& other) :
            intervals(other.intervals),
            left(other.left ? other.left->clone() : nullptr),
            right(other.right ? other.right->clone() : nullptr),
            center(other.center)
        {
        }

        IntervalTree& operator=(IntervalTree&&) = default;
        IntervalTree(IntervalTree&&) = default;

        IntervalTree& operator=(const IntervalTree& other)
        {
            center = other.center;
            intervals = other.intervals;
            left = other.left ? other.left->clone() : nullptr;
            right = other.right ? other.right->clone() : nullptr;
            return *this;
        }

        IntervalTree(
            interval_vector&& ivals,
            std::size_t depth = 16,
            std::size_t minbucket = 64,
            std::size_t maxbucket = 512,
            Scalar leftextent = {},
            Scalar rightextent = {}) :
            left(nullptr), right(nullptr)
        {
            --depth;
            const auto minmaxStop = std::minmax_element(ivals.begin(), ivals.end(), IntervalStopCmp());
            const auto minmaxStart = std::minmax_element(ivals.begin(), ivals.end(), IntervalStartCmp());
            if (!ivals.empty())
            {
                center = (minmaxStart.first->start + minmaxStop.second->stop) / 2;
            }
            if (leftextent == Scalar{} && rightextent == Scalar{})
            {
                // sort intervals by start
                std::sort(ivals.begin(), ivals.end(), IntervalStartCmp());
            }
            else
            {
                assert(std::is_sorted(ivals.begin(), ivals.end(), IntervalStartCmp()));
            }
            if (depth == 0 || (ivals.size() < minbucket && ivals.size() < maxbucket))
            {
                std::sort(ivals.begin(), ivals.end(), IntervalStartCmp());
                intervals = std::move(ivals);
                assert(is_valid().first);
                return;
            }
            else
            {
                Scalar leftp = Scalar{};
                Scalar rightp = Scalar{};

                if (leftextent != Scalar{} || rightextent != Scalar{})
                {
                    leftp = leftextent;
                    rightp = rightextent;
                }
                else
                {
                    leftp = ivals.front().start;
                    rightp = std::max_element(ivals.begin(), ivals.end(), IntervalStopCmp())->stop;
                }

                interval_vector lefts;
                interval_vector rights;

                for (typename interval_vector::const_iterator i = ivals.begin();
                     i != ivals.end();
                     ++i)
                {
                    const interval& interval = *i;
                    if (interval.stop < center)
                    {
                        lefts.push_back(interval);
                    }
                    else if (interval.start > center)
                    {
                        rights.push_back(interval);
                    }
                    else
                    {
                        assert(interval.start <= center);
                        assert(center <= interval.stop);
                        intervals.push_back(interval);
                    }
                }

                if (!lefts.empty())
                {
                    left.reset(new IntervalTree(std::move(lefts),
                                                depth,
                                                minbucket,
                                                maxbucket,
                                                leftp,
                                                center));
                }
                if (!rights.empty())
                {
                    right.reset(new IntervalTree(std::move(rights),
                                                 depth,
                                                 minbucket,
                                                 maxbucket,
                                                 center,
                                                 rightp));
                }
            }
            assert(is_valid().first);
        }

        // Call f on all intervals near the range [start, stop]:
        template<class UnaryFunction>
        void visit_near(const Scalar& start, const Scalar& stop, UnaryFunction f) const
        {
            if (!intervals.empty() && !(stop < intervals.front().start))
            {
                for (auto& i : intervals)
                {
                    f(i);
                }
            }
            if (left && start <= center)
            {
                left->visit_near(start, stop, f);
            }
            if (right && stop >= center)
            {
                right->visit_near(start, stop, f);
            }
        }

        // Call f on all intervals crossing pos
        template<class UnaryFunction>
        void visit_overlapping(const Scalar& pos, UnaryFunction f) const
        {
            visit_overlapping(pos, pos, f);
        }

        // Call f on all intervals overlapping [start, stop]
        template<class UnaryFunction>
        void visit_overlapping(const Scalar& start, const Scalar& stop, UnaryFunction f) const
        {
            auto filterF = [&](const interval& interval) {
                if (interval.stop >= start && interval.start <= stop)
                {
                    // Only apply f if overlapping
                    f(interval);
                }
            };
            visit_near(start, stop, filterF);
        }

        // Call f on all intervals contained within [start, stop]
        template<class UnaryFunction>
        void visit_contained(const Scalar& start, const Scalar& stop, UnaryFunction f) const
        {
            auto filterF = [&](const interval& interval) {
                if (start <= interval.start && interval.stop <= stop)
                {
                    f(interval);
                }
            };
            visit_near(start, stop, filterF);
        }

        interval_vector findOverlapping(const Scalar& start, const Scalar& stop) const
        {
            interval_vector result;
            visit_overlapping(start, stop, [&](const interval& interval) {
                result.emplace_back(interval);
            });
            return result;
        }

        interval_vector findContained(const Scalar& start, const Scalar& stop) const
        {
            interval_vector result;
            visit_contained(start, stop, [&](const interval& interval) {
                result.push_back(interval);
            });
            return result;
        }
        bool empty() const
        {
            if (left && !left->empty())
            {
                return false;
            }
            if (!intervals.empty())
            {
                return false;
            }
            if (right && !right->empty())
            {
                return false;
            }
            return true;
        }

        template<class UnaryFunction>
        void visit_all(UnaryFunction f) const
        {
            if (left)
            {
                left->visit_all(f);
            }
            std::for_each(intervals.begin(), intervals.end(), f);
            if (right)
            {
                right->visit_all(f);
            }
        }

        std::pair<Scalar, Scalar> extentBruitForce() const
        {
            struct Extent
            {
                std::pair<Scalar, Scalar> x = { std::numeric_limits<Scalar>::max(),
                                                std::numeric_limits<Scalar>::min() };
                void operator()(const interval& interval)
                {
                    x.first = std::min(x.first, interval.start);
                    x.second = std::max(x.second, interval.stop);
                }
            };
            Extent extent;

            visit_all([&](const interval& interval) { extent(interval); });
            return extent.x;
        }

        // Check all constraints.
        // If first is false, second is invalid.
        std::pair<bool, std::pair<Scalar, Scalar>> is_valid() const
        {
            const auto minmaxStop = std::minmax_element(intervals.begin(), intervals.end(), IntervalStopCmp());
            const auto minmaxStart = std::minmax_element(intervals.begin(), intervals.end(), IntervalStartCmp());

            std::pair<bool, std::pair<Scalar, Scalar>> result = { true, { std::numeric_limits<Scalar>::max(), std::numeric_limits<Scalar>::min() } };
            if (!intervals.empty())
            {
                result.second.first = std::min(result.second.first, minmaxStart.first->start);
                result.second.second = std::min(result.second.second, minmaxStop.second->stop);
            }
            if (left)
            {
                auto valid = left->is_valid();
                result.first &= valid.first;
                result.second.first = std::min(result.second.first, valid.second.first);
                result.second.second = std::min(result.second.second, valid.second.second);
                if (!result.first)
                {
                    return result;
                }
                if (valid.second.second >= center)
                {
                    result.first = false;
                    return result;
                }
            }
            if (right)
            {
                auto valid = right->is_valid();
                result.first &= valid.first;
                result.second.first = std::min(result.second.first, valid.second.first);
                result.second.second = std::min(result.second.second, valid.second.second);
                if (!result.first)
                {
                    return result;
                }
                if (valid.second.first <= center)
                {
                    result.first = false;
                    return result;
                }
            }
            if (!std::is_sorted(intervals.begin(), intervals.end(), IntervalStartCmp()))
            {
                result.first = false;
            }
            return result;
        }

        friend std::ostream& operator<<(std::ostream& os, const IntervalTree& itree)
        {
            return writeOut(os, itree);
        }

        friend std::ostream& writeOut(std::ostream& os, const IntervalTree& itree, std::size_t depth = 0)
        {
            auto pad = [&]() { for (std::size_t i = 0; i != depth; ++i) { os << ' '; } };
            pad();
            os << "center: " << itree.center << '\n';
            for (const interval& inter : itree.intervals)
            {
                pad();
                os << inter << '\n';
            }
            if (itree.left)
            {
                pad();
                os << "left:\n";
                writeOut(os, *itree.left, depth + 1);
            }
            else
            {
                pad();
                os << "left: nullptr\n";
            }
            if (itree.right)
            {
                pad();
                os << "right:\n";
                writeOut(os, *itree.right, depth + 1);
            }
            else
            {
                pad();
                os << "right: nullptr\n";
            }
            return os;
        }

    private:
        interval_vector intervals;
        std::unique_ptr<IntervalTree> left;
        std::unique_ptr<IntervalTree> right;
        Scalar center;
    };
#ifdef USE_INTERVAL_TREE_NAMESPACE
}
#endif

#endif
