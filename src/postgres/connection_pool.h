#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <pqxx/pqxx>
#include <vector>

namespace postgres {

class ConnectionPool {
    using PoolType = ConnectionPool;
    using ConnectionPtr = std::unique_ptr<pqxx::connection>;

   public:
    class ConnectionWrapper {
       public:
        ConnectionWrapper(ConnectionPtr&& conn, PoolType& pool) noexcept;

        ConnectionWrapper(const ConnectionWrapper&) = delete;
        ConnectionWrapper& operator=(const ConnectionWrapper&) = delete;

        ConnectionWrapper(ConnectionWrapper&&) = default;
        ConnectionWrapper& operator=(ConnectionWrapper&&) = default;

        pqxx::connection& operator*() const& noexcept;
        pqxx::connection& operator*() const&& = delete;

        pqxx::connection* operator->() const& noexcept;

        ~ConnectionWrapper();

       private:
        ConnectionPtr conn_;
        PoolType* pool_;
    };

    // ConnectionFactory is a functional object returning
    // std::shared_ptr<pqxx::connection>
    template <typename ConnectionFactory>
    ConnectionPool(size_t capacity, ConnectionFactory&& connection_factory) {
        pool_.reserve(capacity);
        for (size_t i = 0; i < capacity; ++i) {
            pool_.emplace_back(connection_factory());
        }
    }

    ConnectionWrapper GetConnection();

   private:
    void ReturnConnection(ConnectionPtr&& conn);

    std::mutex mutex_;
    std::condition_variable cond_var_;
    std::vector<ConnectionPtr> pool_;
    size_t used_connections_ = 0;
};

}  // namespace postgres
