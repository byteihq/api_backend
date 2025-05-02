#pragma once

#include <domain/unit_of_work.h>

#include <pqxx/connection>
#include <pqxx/transaction>
#include <string_view>
#include <vector>

namespace postgres {
class UnitOfWorkImpl final : public domain::UnitOfWork {
   public:
    explicit UnitOfWorkImpl(pqxx::connection& conn) noexcept;
    void InitPlayerDB() override;
    std::vector<domain::RetiredPlayer> GetTopKPlayers(
        size_t start, size_t max_count) override;
    void IncreasePlayerStats(std::string_view name, size_t scores,
                             size_t play_time) override;

   private:
    template <typename Callable>
    void TryExec(Callable&& callable) {
        pqxx::work tx{conn_};
        try {
            callable(tx);
        } catch (...) {
            tx.abort();
            throw;
        }
        tx.commit();
    }

   private:
    pqxx::connection& conn_;
};
}  // namespace postgres
