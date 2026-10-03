#pragma once

#include <array>
#include <atomic>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <utility>

namespace ui::utils
{

/**
 * @brief 有界无锁 MPSC 队列。
 *
 * 多生产者通过 CAS 竞争推进入队位置，单消费者独占推进出队位置。
 * 每个槽位维护独立序号，用于判断槽位是否可写、可读或队列是否已满。
 *
 * 注意：
 * - 仅允许一个消费者线程调用 TryDequeue；
 * - 容量会向上取整为 2 的幂，以便用掩码快速定位槽位；
 * - TryEnqueue/TryEmplace 在队列满时立即返回 false；
 * - TryDequeue 在队列空时立即返回 std::nullopt。
 */
template <typename T>
class MpscQueue final
{
   public:
    explicit MpscQueue(std::size_t capacity)
        : kCapacity(NormalizeCapacity(capacity)),
          kMask(kCapacity - 1),
          buffer_(std::make_unique<
                  Cell[]>(  // NOLINT(modernize-avoid-c-arrays,cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays)
              kCapacity))
    {  // NOLINT(modernize-avoid-c-arrays,cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays)
        for (std::size_t index = 0; index < kCapacity; ++index)
        {
            buffer_[index].sequence.store(index, std::memory_order_relaxed);
        }

        enqueue_pos_.store(0, std::memory_order_relaxed);
        dequeue_pos_.store(0, std::memory_order_relaxed);
    }

    MpscQueue(const MpscQueue&) = delete;
    MpscQueue& operator=(const MpscQueue&) = delete;
    MpscQueue(MpscQueue&&) = delete;
    MpscQueue& operator=(MpscQueue&&) = delete;

    ~MpscQueue()
    {
        DrainRemaining();
    }

    /**
     * @brief 尝试原地构造入队，队列已满时返回 false。
     */
    template <typename... Args>
        requires std::constructible_from<T, Args...> && std::is_nothrow_constructible_v<T, Args...>
    [[nodiscard]] bool TryEmplace(Args&&... args)
    {
        return TryEmplaceBeforePublish([]() noexcept {}, std::forward<Args>(args)...);
    }

    /**
     * @brief 原地构造元素，并在槽位发布给消费者前执行提交回调。
     *
     * 回调必须 noexcept；它用于同步与队列发布不可分割的外部记账。回调返回后，
     * release store 才会令槽位对消费者可见。
     */
    template <typename BeforePublish, typename... Args>
        requires std::constructible_from<T, Args...> && std::is_nothrow_constructible_v<T, Args...> &&
             std::invocable<BeforePublish&> && std::is_nothrow_invocable_v<BeforePublish&>
    [[nodiscard]] bool TryEmplaceBeforePublish(BeforePublish&& beforePublish, Args&&... args)
    {
        Cell* cell = nullptr;
        std::size_t position = enqueue_pos_.load(std::memory_order_relaxed);

        while (true)
        {
            cell = &buffer_[position & kMask];
            const std::size_t kSequence = cell->sequence.load(std::memory_order_acquire);
            const auto kDifference = static_cast<std::intptr_t>(kSequence) - static_cast<std::intptr_t>(position);

            if (kDifference == 0)
            {
                if (enqueue_pos_.compare_exchange_weak(position, position + 1, std::memory_order_relaxed,
                                                       std::memory_order_relaxed))
                {
                    break;
                }
            }
            else if (kDifference < 0)
            {
                return false;
            }
            else
            {
                position = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }

        std::construct_at(cell->StoragePtr(), std::forward<Args>(args)...);
        std::invoke(std::forward<BeforePublish>(beforePublish));
        cell->sequence.store(position + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief 尝试复制入队，队列已满时返回 false。
     */
    [[nodiscard]] bool TryEnqueue(const T& value)
    {
        return TryEmplace(value);
    }

    /**
     * @brief 尝试移动入队，队列已满时返回 false。
     */
    [[nodiscard]] bool TryEnqueue(T&& value)
    {
        return TryEmplace(std::move(value));
    }

    /**
     * @brief 尝试出队，队列为空时返回 std::nullopt。
     *
     * 该函数只能由单个消费者线程调用。
     */
    [[nodiscard]] std::optional<T> TryDequeue()
    {
        const std::size_t kPosition = dequeue_pos_.load(std::memory_order_relaxed);
        Cell& cell = buffer_[kPosition & kMask];
        const std::size_t kSequence = cell.sequence.load(std::memory_order_acquire);
        const auto kDifference = static_cast<std::intptr_t>(kSequence) - static_cast<std::intptr_t>(kPosition + 1);

        if (kDifference < 0)
        {
            return std::nullopt;
        }

        auto* value = cell.StoragePtr();
        std::optional<T> result(std::move(*value));
        std::destroy_at(value);
        cell.sequence.store(kPosition + kCapacity, std::memory_order_release);
        dequeue_pos_.store(kPosition + 1, std::memory_order_relaxed);
        return result;
    }

    /**
     * @brief 队列容量。
     */
    [[nodiscard]] std::size_t Capacity() const noexcept
    {
        return kCapacity;
    }

    /**
     * @brief 估算当前元素数量。并发场景下仅用于观测，不适合作为同步条件。
     */
    [[nodiscard]] std::size_t ApproximateSize() const noexcept
    {
        const auto kEnqueuePosition = enqueue_pos_.load(std::memory_order_relaxed);
        const auto kDequeuePosition = dequeue_pos_.load(std::memory_order_relaxed);
        return kEnqueuePosition >= kDequeuePosition ? kEnqueuePosition - kDequeuePosition : 0;
    }

   private:
    /**
     * @brief 队列槽位，包含元素存储和槽位序号。
     */
    struct Cell
    {
        alignas(64) std::atomic<std::size_t> sequence{0};
        alignas(T) std::array<std::byte, sizeof(T)> storage{};

        [[nodiscard]] T* StoragePtr() noexcept
        {
            return std::launder(
                reinterpret_cast<T*>(storage.data()));  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        }
    };

    static std::size_t NormalizeCapacity(std::size_t capacity)
    {
        if (capacity == 0)
        {
            throw std::invalid_argument("MpscQueue capacity must be greater than zero");
        }

        if (capacity > (std::size_t{1} << ((sizeof(std::size_t) * 8) - 1)))
        {
            throw std::invalid_argument("MpscQueue capacity is too large");
        }

        return std::bit_ceil(capacity);
    }

    void DrainRemaining() noexcept
    {
        while (TryDequeue().has_value())
        {
            // 继续出队直到队列为空
        }
    }

    static constexpr std::size_t kCachelineSize = 64;
    const std::size_t kCapacity;
    const std::size_t kMask;
    std::unique_ptr<Cell[]>
        buffer_;  // NOLINT(modernize-avoid-c-arrays,cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays)
    alignas(kCachelineSize) std::atomic<std::size_t> enqueue_pos_{0};
    alignas(kCachelineSize) std::atomic<std::size_t> dequeue_pos_{0};
};

}  // namespace ui::utils
