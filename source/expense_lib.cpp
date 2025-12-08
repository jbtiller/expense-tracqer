#include <cstddef>
#include <filesystem>
#include <format>
#include <iomanip>
#include <iostream>
#include <stdexcept>

#include <sqlite3.h>

#include "expense_lib.hpp"

static const char* const db_schema =
R"(CREATE TABLE IF NOT EXISTS "expenses_account" (
    "id" integer PRIMARY KEY AUTOINCREMENT,
    "name" varchar(128) UNIQUE NOT NULL
);
CREATE TABLE IF NOT EXISTS "expenses_category" (
    "id" integer PRIMARY KEY AUTOINCREMENT,
    "name" varchar(128) UNIQUE NOT NULL
);
CREATE TABLE IF NOT EXISTS "expenses_payee" (
    "id" integer PRIMARY KEY AUTOINCREMENT,
    "name" varchar(128) UNIQUE NOT NULL
);
CREATE TABLE IF NOT EXISTS "expenses_payment" (
    "id" integer PRIMARY KEY AUTOINCREMENT,
    "paid_date" integer NOT NULL,
    "amount_cents" integer NOT NULL,
    "account" integer NOT NULL REFERENCES "expenses_account" ("id"),
    "category" integer NOT NULL REFERENCES "expenses_category" ("id"),
    "payee" integer NOT NULL REFERENCES "expenses_payee" ("id")
);
CREATE INDEX "expenses_payment_account_2d96e66d" ON "expenses_payment" ("account");
CREATE INDEX "expenses_payment_category_82362a95" ON "expenses_payment" ("category");
CREATE INDEX "expenses_payment_payee_a6e0a06b" ON "expenses_payment" ("payee");
)";

Expenses::Expenses() {
    auto new_db = !std::filesystem::exists("test.db");

    std::cout << "Expenses::Expenses: Opening 'test.db'.\n";
    db_open("test.db");
    if (new_db) {
        std::cout << "Expenses::Expenses: Creating database schema.\n";
        db_create_schema();
    }
}

Expenses::~Expenses() {
    sqlite3_close(m_db);
}

auto Expenses::db_open(const std::string& db_filename) -> void {
    int ret = sqlite3_open(db_filename.c_str(), &m_db);
    if (ret == SQLITE_OK) {
        std::cout << "Expenses::db_open: Db opened successfully.\n";
    } else {
        std::cerr << "Expenses::db_open: Error opening database (ret=" << ret << ", '"
                  << std::quoted(sqlite3_errmsg(m_db)) << "')\n";
        throw std::runtime_error("Error opening database");
    }
}

auto Expenses::db_create_schema() -> void {
  char* errmsg {};
  int ret = sqlite3_exec(m_db, db_schema, nullptr, nullptr, &errmsg);
  if (ret == SQLITE_OK) {
      std::cout << "Expenses::db_create_schema: Db schema created successfuly.\n";
  } else {
        std::cerr << "Expenses::db_create_schema: Error creationg db schema (ret=" << ret << ", '"
                  << std::quoted(errmsg) << "')\n";
        sqlite3_free(errmsg);
        throw std::runtime_error("Error opening database");
  }
}

static auto handle_int_string_cb(void* data, int argc, char** argv, char** col_name) -> int {
    auto& accounts = *static_cast<std::map<int,std::string>*>(data);
    accounts[std::stoi(argv[0])] = argv[1];
    return 0;
}

auto Expenses::get_string_table(const std::string& table_name) -> std::map<int,std::string> {
    char* errmsg {};
    std::map<int,std::string> results;
    auto sql = std::format("SELECT * FROM {};", table_name);
    auto ret = sqlite3_exec(m_db, sql.c_str(), handle_int_string_cb, static_cast<void*>(&results), &errmsg);
    if (ret != SQLITE_OK) {
        std::cerr << "Expenses::get_string_table: Error querying table " << table_name << " (ret="
                  << ret << ", " << std::quoted(errmsg) << ")\n";
    }
    return results;
}

auto Expenses::get_accounts() -> std::map<int,std::string> {
    return get_string_table("expenses_account");
}

auto Expenses::get_categories() -> std::map<int,std::string> {
    return get_string_table("expenses_category");
}

auto Expenses::get_payees() -> std::map<int,std::string> {
    return get_string_table("expenses_payee");
}

auto get_single_int(void* data, int argc, char** argv, char** col_name) -> int {
    auto* ret = static_cast<int*>(data);
    *ret = std::stoi(argv[0]);
    return 0;
}

auto get_payment(void* data, int argc, char** argv, char** col_name) -> int {
    auto* payments = static_cast<std::map<int,Expenses::Payment>*>(data);
    auto id = std::stoi(argv[0]);
    auto paid_date = std::stoi(argv[1]);
    auto amount_cents = std::stoi(argv[2]);
    auto account_id = std::stoi(argv[3]);
    auto category_id = std::stoi(argv[4]);
    auto payee_id = std::stoi(argv[5]);
    payments->insert({id, {.date = paid_date,
                           .amount_cents = amount_cents,
                           .account_id = account_id,
                           .category_id = category_id,
                           .payee_id = payee_id}});
    return 0;
}

auto Expenses::get_payments() -> std::map<int,Expenses::Payment> {
    // Get number of payments.
    int num_payments {};
    char* errmsg {};
    auto ret = sqlite3_exec(m_db, "SELECT COUNT(*) FROM expenses_payment;", get_single_int, &num_payments, &errmsg);
    if (ret != SQLITE_OK) {
        std::cerr << "Expenses::get_payments: DB error querying number of payments (" << ret
                  << ", " << std::quoted(errmsg) << ")\n";
        return {};
    }

    // Get all payments.
    std::map<int,Expenses::Payment> payments {};
    ret = sqlite3_exec(m_db,
                       "SELECT id, paid_date, amount_cents, account, category, payee FROM expenses_payment;",
                       get_payment,
                       &payments,
                       &errmsg);
    if (ret != SQLITE_OK) {
        std::cerr << "Expenses::get_payments: DB error retrieving payments (" << ret
                  << ", " << std::quoted(errmsg) << ")\n";
        return {};
    }

    return payments;
}

auto Expenses::add_name(const Expenses::TableName& table_name,
                        const Expenses::Name& name) -> std::optional<int> {
    sqlite3_stmt* ins_stmt {};
    const auto ins_sql = std::format("INSERT INTO {} (name) VALUES (?) RETURNING id;", table_name.cref());
    auto ret = sqlite3_prepare_v2(m_db, ins_sql.c_str(), -1, &ins_stmt, NULL);
    if (ret != SQLITE_OK) {
        std::cerr << std::format("Error preparing insert statement. table={}, err={} {}\n",
                                 table_name.cref(), ret, sqlite3_errmsg(m_db));
        sqlite3_finalize(ins_stmt);
        return {};
    }

    // SQL parameter index starts at 1, not 0.
    ret = sqlite3_bind_text(ins_stmt, 1, name.cref().c_str(), -1, SQLITE_STATIC);
    if (ret != SQLITE_OK) {
        std::cerr << std::format("Error beinding string to insert statement. table={}, err={} {}\n",
                                 table_name.cref(), ret, sqlite3_errmsg(m_db));
        sqlite3_finalize(ins_stmt);
        return {};
    }

    ret = sqlite3_step(ins_stmt);
    // Because we're doing an INSERT and RETURNING, there are is exactly one returned row.
    if (ret != SQLITE_ROW) {
        std::cerr << std::format("Error: first step() didn't return a row. table={}, err={} {}\n",
                                 table_name.cref(), ret, sqlite3_errmsg(m_db));
        sqlite3_finalize(ins_stmt);
        return {};
    }

    // Columns in the result set are numbered from 0.
    auto row_id = sqlite3_column_int(ins_stmt, 0);

    ret = sqlite3_step(ins_stmt);
    sqlite3_finalize(ins_stmt);
    if (ret != SQLITE_DONE) {
        std::cerr << std::format("Error: 2nd step() didn't return DONE. table={}, err={} {}\n",
                                 table_name.cref(), ret, sqlite3_errmsg(m_db));
        return {};
    }

    return row_id;
}

auto Expenses::add_account(const std::string& account_name) -> std::optional<int> {
    return add_name(TableName("expenses_account"), Name(account_name));
}

auto Expenses::add_category(const std::string& category_name) -> std::optional<int> {
    return add_name(TableName("expenses_category"), Name(category_name));
}

auto Expenses::add_payee(const std::string& payee_name) -> std::optional<int> {
    return add_name(TableName("expenses_payee"), Name(payee_name));
}

auto Expenses::add_payment(Expenses::Payment payment) -> std::optional<int> {
    sqlite3_stmt* ins_stmt {};
    const char* ins_sql = "INSERT INTO expenses_payment"
        "        (paid_date, amount_cents, account, category, payee)"
        " VALUES (?,         ?,            ?,       ?,        ?) RETURNING id;";
    //        ^^  1          2             3        4         5 ^^
    auto ret = sqlite3_prepare_v2(m_db, ins_sql, -1, &ins_stmt, NULL);
    if (ret != SQLITE_OK) {
        std::cerr << "Error preparing 'INSERT INTO expenses_payment' statement. err=" << ret << ", "
                  << std::quoted(sqlite3_errmsg(m_db)) << "\n";
        sqlite3_finalize(ins_stmt);
        return {};
    }

    // SQL parameter index starts at 1, not 0.
    auto params = std::array<std::tuple<int, int, const char*>, 5>({
            {1, payment.date, "date"},
            {2, payment.amount_cents, "amount"},
            {3, payment.account_id, "account"},
            {4, payment.category_id, "category"},
            {5, payment.payee_id, "payee"}
        });
    for (auto [param_num, param_val, param_desc] : params) {
        ret = sqlite3_bind_int(ins_stmt, param_num, param_val);
        if (ret != SQLITE_OK) {
            std::cerr << "Error binding " << param_desc << " to INSERT statement. err=" << ret << ", "
                      << std::quoted(sqlite3_errmsg(m_db)) << "\n";
            sqlite3_finalize(ins_stmt);
            return SQLITE_ERROR;
        }
    }

    // Because we're doing an INSERT with a RETURNING id, there should be one row.
    ret = sqlite3_step(ins_stmt);
    if (ret != SQLITE_ROW) {
        std::cerr << "Error: First step() did not return the inserted row ID. ret=" << ret << ", "
                  << std::quoted(sqlite3_errmsg(m_db)) << "\n";
        sqlite3_finalize(ins_stmt);
        return SQLITE_ERROR;
    }
    
    // Only one column in return set.
    auto row_id = sqlite3_column_int(ins_stmt, 0);

    // But just one row.
    ret = sqlite3_step(ins_stmt);
    if (ret != SQLITE_DONE) {
        std::cerr << "Error: Two step()s did not complete the insert. ret=" << ret << ", "
                  << std::quoted(sqlite3_errmsg(m_db)) << "\n";
        sqlite3_finalize(ins_stmt);
        return SQLITE_ERROR;
    }

    sqlite3_finalize(ins_stmt);
    return row_id;
}
