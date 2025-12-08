#pragma once

#include <map>
#include <optional>
#include <string>

#include "wrapper.hpp"

// Forward reference
class sqlite3;

class Expenses {
public:
    struct Payment {
	int date;
	int32_t amount_cents;
	int account_id;
	int category_id;
	int payee_id;
    };
    WRAPPER(TableName, std::string);
    WRAPPER(Name, std::string);

public:
    Expenses();
    ~Expenses();

    auto get_accounts() -> std::map<int,std::string>;
    auto get_categories() -> std::map<int,std::string>;
    auto get_payees() -> std::map<int,std::string>;
    auto get_payments() -> std::map<int,Payment>;

    auto add_account(const std::string& account_name) -> std::optional<int>;
    auto add_category(const std::string& category_name) -> std::optional<int>;
    auto add_payee(const std::string& payee_name) -> std::optional<int>;
    auto add_payment(Payment payment) -> std::optional<int>;

protected:
    // Purely for testing.
    enum class DoNothing { DoNothing };
    explicit Expenses(DoNothing) {
    }

    auto db_open(const std::string& db_filename) -> void;
    auto db_create_schema() -> void;
    auto get_string_table(const std::string& table_name) -> std::map<int,std::string>;

    auto add_name(const TableName& table_name, const Name& name) -> std::optional<int>;

    sqlite3* m_db {};
};
