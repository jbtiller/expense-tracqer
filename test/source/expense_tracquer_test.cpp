#include <cstdlib>
#include <filesystem>
#include <format>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <set>

#include "gtest.h"
#include "sqlite3.h"

#include "expense_lib.hpp"

class ExpensesTester : public Expenses {
public:
    using Expenses::Payment;
    using Expenses::DoNothing;

    ExpensesTester() = default;
    explicit ExpensesTester(DoNothing n) : Expenses(n) {}

    auto db_open(const std::string& db_filename) -> void {
	Expenses::db_open(db_filename);
    }

    auto db_create_schema() -> void {
	Expenses::db_create_schema();
    }

    auto get_accounts() -> std::map<int,std::string> {
        return Expenses::get_accounts();
    }

    auto get_categories() -> std::map<int,std::string> {
        return Expenses::get_categories();
    }

    auto get_payees() -> std::map<int,std::string> {
        return Expenses::get_payees();
    }

    auto get_payments() -> std::map<int,Payment> {
        return Expenses::get_payments();
    }

    using Expenses::TableName;
    using Expenses::Name;
    auto add_name(const TableName& table_name, const Name& name) -> std::optional<int> {
        return Expenses::add_name(table_name, name);
    }

    using Expenses::m_db;
    static const char* m_db_filename;
};
const char* ExpensesTester::m_db_filename {"test.db"};

// This is called once for each row returned by the query. argc is the
// number of columns, argv holds the value of the ith column, and
// col_name holds the name of the ith column.
static int ret_num_rows_cb(void* data, int argc, char** argv, char** col_name) {
    std::cout << "ret_num_rows_cb\n";
    *(static_cast<int*>(data)) += 1;
    return 0;
}

TEST(ExpensesLibTest, db_open_no_file) {
    std::filesystem::remove(ExpensesTester::m_db_filename);

    auto e = ExpensesTester(ExpensesTester::DoNothing::DoNothing);
    ASSERT_NO_THROW(e.db_open(e.m_db_filename));
    EXPECT_TRUE(e.m_db != nullptr);
    EXPECT_TRUE(std::filesystem::exists(e.m_db_filename));

    char* err_msg {};
    int num_tables {};
    // This call may appear to succeed even on an empty database file but the callback won't be run.
    auto ret = sqlite3_exec(e.m_db,
			    "SELECT name FROM sqlite_schema WHERE type = 'table';",
			    ret_num_rows_cb,
			    static_cast<void*>(&num_tables),
			    &err_msg);
    if (ret != SQLITE_OK) {
	std::cout << std::format("Table enumeration query failed code={} str={}\n", ret, err_msg);
	sqlite3_free(err_msg);
    }
    ASSERT_EQ(ret, SQLITE_OK);
    EXPECT_EQ(num_tables, 0);
}

TEST(ExpensesLibTest, db_open_existing_file) {
    std::filesystem::remove(ExpensesTester::m_db_filename);

    sqlite3* db {};
    auto ret = sqlite3_open(ExpensesTester::m_db_filename, &db);
    if (ret != SQLITE_OK) {
	std::cerr << "Failed to open DB using raw sqlite API: code=" << ret << ", msg="
		  << std::quoted(sqlite3_errmsg(db)) << "\n";
	ASSERT_TRUE(false && "db open failed");
    }

    char* err_msg {};
    ret = sqlite3_exec(db,
		       "CREATE TABLE test (id INTEGER);",
		       NULL,
		       NULL,
		       &err_msg);
    if (ret != SQLITE_OK) {
	std::cerr << "Failed to create table using raw sqlite API: code=" << ret << ", msg="
		  << std::quoted(err_msg) << "\n";
	sqlite3_free(err_msg);
	ASSERT_TRUE(false && "create table failed");
    }
    sqlite3_close(db);

    auto e = ExpensesTester(ExpensesTester::DoNothing::DoNothing);

    ASSERT_NO_THROW(e.db_open(e.m_db_filename));
    EXPECT_TRUE(e.m_db != nullptr);
    EXPECT_TRUE(std::filesystem::exists(e.m_db_filename));

    int num_tables {};
    // This call may appear to succeed even on an empty database file but the callback won't be run.
    ret = sqlite3_exec(e.m_db,
			    "SELECT name FROM sqlite_schema WHERE type = 'table';",
			    ret_num_rows_cb,
			    static_cast<void*>(&num_tables),
			    &err_msg);
    if (ret != SQLITE_OK) {
	std::cout << std::format("Table enumeration query failed code={} str={}\n", ret, err_msg);
    }
    ASSERT_EQ(ret, SQLITE_OK);
    EXPECT_EQ(num_tables, 1);
}

static int get_table_names_cb(void* data, int argc, char** argv, char** col_name) {
    std::cout << "get_table_names_cb\n";
    auto& table_names = *static_cast<std::set<std::string>*>(data);
    std::cout << argv[0] << "\n";
    table_names.insert(argv[0]);
    return 0;
}

static auto quote(const std::string& s) -> std::string {
    std::ostringstream oss;
    oss << std::quoted(s);
    return oss.str();
}

static auto insert_names(sqlite3* db, const std::string& table_name, const std::vector<std::string>& names) -> bool {
    sqlite3_stmt* ins_stmt {};
    const auto ins_sql = std::format("INSERT INTO {} (name) VALUES (?);", table_name);
    auto ret = sqlite3_prepare_v2(db, ins_sql.c_str(), -1, &ins_stmt, NULL);
    if (ret != SQLITE_OK) {
        std::cerr << std::format("Error preparing insert statement. table={}, err={} {}\n",
                                 table_name, ret, quote(sqlite3_errmsg(db)));
        sqlite3_finalize(ins_stmt);
        return false;
    }

    for (auto name : names) {
        // SQL parameter index starts at 1, not 0.
        ret = sqlite3_bind_text(ins_stmt, 1, name.c_str(), -1, SQLITE_STATIC);
        if (ret != SQLITE_OK) {
            std::cerr << std::format("Error beinding string to insert statement. table={}, err={} {}\n",
                                     table_name, ret, quote(sqlite3_errmsg(db)));
            sqlite3_finalize(ins_stmt);
            return false;
        }

        ret = sqlite3_step(ins_stmt);
        // Because we're doing an INSERT without a RETURNING, there are no returned rows.
        if (ret != SQLITE_DONE) {
            std::cerr << std::format("Error: step() didn't return DONE. table={}, err={} {}\n",
                                     table_name, ret, quote(sqlite3_errmsg(db)));
            sqlite3_finalize(ins_stmt);
            return false;
        }

        // Returns the result code from the most recent step().
        sqlite3_reset(ins_stmt);
        if (ret != SQLITE_DONE) {
            std::cerr << std::format("Error resetting prepared insert statement. table={}, err={} {}\n",
                                     table_name, ret, quote(sqlite3_errmsg(db)));
            sqlite3_finalize(ins_stmt);
            return false;
        }
    }

    return sqlite3_finalize(ins_stmt) == SQLITE_OK;
}

TEST(ExpensesLibTest, db_create_schema_new_file) {
    std::filesystem::remove(ExpensesTester::m_db_filename);

    auto e = ExpensesTester(ExpensesTester::DoNothing::DoNothing);
    ASSERT_NO_THROW(e.db_open(e.m_db_filename));
    EXPECT_TRUE(e.m_db != nullptr);
    EXPECT_TRUE(std::filesystem::exists(e.m_db_filename));

    ASSERT_NO_THROW(e.db_create_schema());

    char* err_msg {};
    std::set<std::string> table_names;
    // This call may appear to succeed even on an empty database file but the callback won't be run.
    auto ret = sqlite3_exec(e.m_db,
			    "SELECT name FROM sqlite_schema WHERE type = 'table';",
			    get_table_names_cb,
			    static_cast<void*>(&table_names),
			    &err_msg);
    if (ret != SQLITE_OK) {
	std::cout << std::format("Table enumeration query failed code={} str={}\n", ret, err_msg);
	sqlite3_free(err_msg);
    }
    ASSERT_EQ(ret, SQLITE_OK);
    EXPECT_TRUE(table_names.contains("expenses_account"));
    EXPECT_TRUE(table_names.contains("expenses_category"));
    EXPECT_TRUE(table_names.contains("expenses_payee"));
    EXPECT_TRUE(table_names.contains("expenses_payment"));
    
    int num_rows {10};
    auto get_single_int_result = [] (void* num_rows, int, char** argv, char**) -> int {
        *((int*) num_rows) = std::atoi(argv[0]);
        return 0;
    };
    ret = sqlite3_exec(e.m_db, "SELECT COUNT(*) FROM expenses_account;", get_single_int_result, &num_rows, &err_msg);
    EXPECT_EQ(num_rows, 0);
    ret = sqlite3_exec(e.m_db, "SELECT COUNT(*) FROM expenses_category;", get_single_int_result, &num_rows, &err_msg);
    EXPECT_EQ(num_rows, 0);
    ret = sqlite3_exec(e.m_db, "SELECT COUNT(*) FROM expenses_payee;", get_single_int_result, &num_rows, &err_msg);
    EXPECT_EQ(num_rows, 0);
    ret = sqlite3_exec(e.m_db, "SELECT COUNT(*) FROM expenses_payment;", get_single_int_result, &num_rows, &err_msg);
    EXPECT_EQ(num_rows, 0);
}

TEST(ExpensesLibTest, get_accounts) {
    std::filesystem::remove(ExpensesTester::m_db_filename);

    auto e = ExpensesTester(ExpensesTester::DoNothing::DoNothing);
    ASSERT_NO_THROW(e.db_open(e.m_db_filename));
    EXPECT_TRUE(e.m_db != nullptr);
    EXPECT_TRUE(std::filesystem::exists(e.m_db_filename));

    ASSERT_NO_THROW(e.db_create_schema());

    // Insert a few rows into the name table.
    std::vector<std::string> exp_names {"Provident", "Citibank"};
    ASSERT_TRUE(insert_names(e.m_db, "expenses_account", exp_names));

    auto names = e.get_accounts();
    ASSERT_EQ(names.size(), exp_names.size());
    for (auto [id, name] : names) {
        auto name_it = std::find(exp_names.begin(), exp_names.end(), name);
        EXPECT_NE(name_it, exp_names.end());
        exp_names.erase(name_it);
    }
}

// This is a copy-pasta of get_accounts. ::sigh:: Yeah, I suck.
TEST(ExpensesLibTest, get_categories) {
    std::filesystem::remove(ExpensesTester::m_db_filename);

    auto e = ExpensesTester(ExpensesTester::DoNothing::DoNothing);
    ASSERT_NO_THROW(e.db_open(e.m_db_filename));
    EXPECT_TRUE(e.m_db != nullptr);
    EXPECT_TRUE(std::filesystem::exists(e.m_db_filename));

    ASSERT_NO_THROW(e.db_create_schema());

    // Insert a few rows into the name table.
    std::vector<std::string> exp_names {"Dining", "Concerts"};
    ASSERT_TRUE(insert_names(e.m_db, "expenses_category", exp_names));

    auto names = e.get_categories();
    ASSERT_EQ(names.size(), 2);
    for (auto const& [id, name] : names) {
        auto name_it = std::find(exp_names.begin(), exp_names.end(), name);
        EXPECT_NE(name_it, exp_names.end());
        exp_names.erase(name_it);
    }
}

TEST(ExpensesLibTest, get_payees) {
    std::filesystem::remove(ExpensesTester::m_db_filename);

    auto e = ExpensesTester(ExpensesTester::DoNothing::DoNothing);
    ASSERT_NO_THROW(e.db_open(e.m_db_filename));
    EXPECT_TRUE(e.m_db != nullptr);
    EXPECT_TRUE(std::filesystem::exists(e.m_db_filename));

    ASSERT_NO_THROW(e.db_create_schema());

    // Insert a few rows into the name table.
    std::vector<std::string> exp_names {"Jay Patel", "Jose Ramos"};
    ASSERT_TRUE(insert_names(e.m_db, "expenses_payee", exp_names));

    auto names = e.get_payees();
    ASSERT_EQ(names.size(), exp_names.size());
    for (auto [id, name] : names) {
        auto name_it = std::find(exp_names.begin(), exp_names.end(), name);
        EXPECT_NE(name_it, exp_names.end());
        exp_names.erase(name_it);
    }
}

static int get_single_int(void* data, int argc, char** argv, char** col_name) {
    *static_cast<int*>(data) = std::stoi(argv[0]);
    return 0;
}


static int get_name_row_id(sqlite3* db, const std::string& table_name, const std::string& name, int* result) {
    const auto sql = std::format("SELECT id FROM {} WHERE name = ?;", table_name);
    sqlite3_stmt* stmt {};
    auto ret = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        std::cerr << std::format("Error preparing select statement. table={}, err={} {}\n",
                                 table_name, ret, quote(sqlite3_errmsg(db)));
        sqlite3_finalize(stmt);
        return SQLITE_ERROR;
    }

    // SQL parameter index starts at 1, not 0.
    ret = sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    if (ret != SQLITE_OK) {
        std::cerr << std::format("Error beinding name to select statement. table={}, err={} {}\n",
                                 table_name, ret, quote(sqlite3_errmsg(db)));
        sqlite3_finalize(stmt);
        return SQLITE_ERROR;
    }

    // We're retrieving a single value, so the return must indicate a row is available.
    ret = sqlite3_step(stmt);
    if (ret != SQLITE_ROW) {
        std::cerr << std::format("Error: first step() didn't return a row. table={}, err={} {}\n",
                                 table_name, ret, quote(sqlite3_errmsg(db)));
        sqlite3_finalize(stmt);
        return SQLITE_ERROR;
    }
    // Returned column sets start at index 0. Stupid SQL.
    auto row_id = sqlite3_column_int(stmt, 0);

    // Because we get just one row, the next step should indicate we're done.
    ret = sqlite3_step(stmt);
    if (ret != SQLITE_DONE) {
        std::cerr << std::format("Error: step() didn't return DONE. table={}, err={} {}\n",
                                 table_name, ret, quote(sqlite3_errmsg(db)));
        sqlite3_finalize(stmt);
        return SQLITE_ERROR;
    }

    sqlite3_finalize(stmt);
    *result = row_id;
    return SQLITE_OK;
}

int insert_payment(sqlite3* db, int date, int amount_cents, int account_id, int category_id, int payee_id, int* row_id) {
    sqlite3_stmt* ins_stmt {};
    const char* ins_sql = "INSERT INTO expenses_payment"
        "        (paid_date, amount_cents, account, category, payee)"
        " VALUES (?,         ?,            ?,       ?,        ?) RETURNING id;";
    //        ^^  1          2             3        4         5 ^^
    auto ret = sqlite3_prepare_v2(db, ins_sql, -1, &ins_stmt, NULL);
    if (ret != SQLITE_OK) {
        std::cerr << "Error preparing 'INSERT INTO expenses_payment' statement. err=" << ret << ", "
                  << std::quoted(sqlite3_errmsg(db)) << "\n";
        sqlite3_finalize(ins_stmt);
        return SQLITE_ERROR;
    }

    // SQL parameter index starts at 1, not 0.
    auto params = std::array<std::tuple<int, int, const char*>, 5>({
            {1, date, "date"},
            {2, amount_cents, "amount"},
            {3, account_id, "account"},
            {4, category_id, "category"},
            {5, payee_id, "payee"}
        });
    for (auto [param_num, param_val, param_desc] : params) {
        ret = sqlite3_bind_int(ins_stmt, param_num, param_val);
        if (ret != SQLITE_OK) {
            std::cerr << "Error binding " << param_desc << " to INSERT statement. err=" << ret << ", "
                      << std::quoted(sqlite3_errmsg(db)) << "\n";
            sqlite3_finalize(ins_stmt);
            return SQLITE_ERROR;
        }
    }

    // Because we're doing an INSERT with a RETURNING id, there should be one row.
    ret = sqlite3_step(ins_stmt);
    if (ret != SQLITE_ROW) {
        std::cerr << "Error: First step() did not return the inserted row ID. ret=" << ret << ", "
                  << std::quoted(sqlite3_errmsg(db)) << "\n";
        sqlite3_finalize(ins_stmt);
        return SQLITE_ERROR;
    }
    
    // Only one column in return set.
    *row_id = sqlite3_column_int(ins_stmt, 0);

    // But just one row.
    ret = sqlite3_step(ins_stmt);
    if (ret != SQLITE_DONE) {
        std::cerr << "Error: Two step()s did not complete the insert. ret=" << ret << ", "
                  << std::quoted(sqlite3_errmsg(db)) << "\n";
        sqlite3_finalize(ins_stmt);
        return SQLITE_ERROR;
    }

    return sqlite3_finalize(ins_stmt);
}

TEST(ExpensesLibTest, get_payments) {
    std::filesystem::remove(ExpensesTester::m_db_filename);

    auto e = ExpensesTester(ExpensesTester::DoNothing::DoNothing);
    ASSERT_NO_THROW(e.db_open(e.m_db_filename));
    EXPECT_TRUE(e.m_db != nullptr);
    EXPECT_TRUE(std::filesystem::exists(e.m_db_filename));

    ASSERT_NO_THROW(e.db_create_schema());

    // Insert a few rows into the names tables.
    auto exp_accounts = std::vector<std::string>({"Provident", "PayPal"});
    auto exp_categories = std::vector<std::string>({"Home Maintenance", "Concerts"});
    auto exp_payees = std::vector<std::string>({"Mr Cooper", "Chanticleer"});
    ASSERT_TRUE(insert_names(e.m_db, "expenses_account", exp_accounts));
    ASSERT_TRUE(insert_names(e.m_db, "expenses_category", exp_categories));
    ASSERT_TRUE(insert_names(e.m_db, "expenses_payee", exp_payees));

    // Get the ids of the name rows we added.
    int acc_provident_id {};
    int acc_paypal_id {};
    int cat_home_id {};
    int cat_concerts_id {};
    int pay_cooper_id {};
    int pay_chanticleer_id {};

    auto ret = get_name_row_id(e.m_db, "expenses_account", "Provident", &acc_provident_id);
    ASSERT_EQ(ret, SQLITE_OK);
    ret = get_name_row_id(e.m_db, "expenses_account", "PayPal", &acc_paypal_id);
    ASSERT_EQ(ret, SQLITE_OK);
    ret = get_name_row_id(e.m_db, "expenses_category", "Home Maintenance", &cat_home_id);
    ASSERT_EQ(ret, SQLITE_OK);
    ret = get_name_row_id(e.m_db, "expenses_category", "Concerts", &cat_concerts_id);
    ASSERT_EQ(ret, SQLITE_OK);
    ret = get_name_row_id(e.m_db, "expenses_payee", "Mr Cooper", &pay_cooper_id);
    ASSERT_EQ(ret, SQLITE_OK);
    ret = get_name_row_id(e.m_db, "expenses_payee", "Chanticleer", &pay_chanticleer_id);
    ASSERT_EQ(ret, SQLITE_OK);
    
    // Insert a few transactions.
    int cooper_trans_id {};
    int chanticleer_trans_id {};
    auto cooper_date = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    int cooper_amount = 106800;
    ret = insert_payment(e.m_db, cooper_date, cooper_amount, acc_provident_id, cat_home_id, pay_cooper_id, &cooper_trans_id);
    ASSERT_EQ(ret, SQLITE_OK);
    auto chanticleer_date = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    int chanticleer_amount = 6500;
    ret = insert_payment(e.m_db, chanticleer_date, chanticleer_amount, acc_paypal_id, cat_concerts_id, pay_chanticleer_id, &chanticleer_trans_id);
    ASSERT_EQ(ret, SQLITE_OK);

    auto payments = e.get_payments();
    std::cout << "cooper_trans_id=" << cooper_trans_id << ", chanticleer_trans_id=" << chanticleer_trans_id << "\n";
    std::cout << "payments\n";
    for (auto [row_id, payment] : payments) {
        std::cout << row_id << ": date=" << payment.date
                  << ", amount_cents=" << payment.amount_cents
                  << ", account_id=" << payment.account_id
                  << ", category_id=" << payment.category_id
                  << ", payee_id=" << payment.payee_id
                  << "\n";
    }
    ASSERT_EQ(payments.size(), 2);
    auto cooper_payment = payments.at(cooper_trans_id);
    EXPECT_EQ(cooper_payment.date, cooper_date);
    EXPECT_EQ(cooper_payment.amount_cents, cooper_amount);
    EXPECT_EQ(cooper_payment.account_id, acc_provident_id);
    EXPECT_EQ(cooper_payment.category_id, cat_home_id);
    EXPECT_EQ(cooper_payment.payee_id, pay_cooper_id);

    auto chanticleer_payment = payments.at(chanticleer_trans_id);
    EXPECT_EQ(chanticleer_payment.date, chanticleer_date);
    EXPECT_EQ(chanticleer_payment.amount_cents, chanticleer_amount);
    EXPECT_EQ(chanticleer_payment.account_id, acc_paypal_id);
    EXPECT_EQ(chanticleer_payment.category_id, cat_concerts_id);
    EXPECT_EQ(chanticleer_payment.payee_id, pay_chanticleer_id);
}

TEST(ExpensesLibTest, add_name) {
    std::filesystem::remove(ExpensesTester::m_db_filename);

    auto e = ExpensesTester(ExpensesTester::DoNothing::DoNothing);
    ASSERT_NO_THROW(e.db_open(e.m_db_filename));
    EXPECT_TRUE(e.m_db != nullptr);
    EXPECT_TRUE(std::filesystem::exists(e.m_db_filename));

    ASSERT_NO_THROW(e.db_create_schema());

    ASSERT_TRUE(e.add_name(ExpensesTester::TableName("expenses_payee"), ExpensesTester::Name("Garth Cummings")));

    auto names = std::set<std::string>{};
    char* errmsg {};
    auto ret = sqlite3_exec(e.m_db, "SELECT name FROM expenses_payee;", get_table_names_cb, &names, &errmsg);
    ASSERT_EQ(ret, SQLITE_OK);

    ASSERT_EQ(names.size(), 1);
    EXPECT_TRUE(names.contains("Garth Cummings"));
}

TEST(ExpensesLibTest, add_account) {
    std::filesystem::remove(ExpensesTester::m_db_filename);

    auto e = ExpensesTester(ExpensesTester::DoNothing::DoNothing);
    ASSERT_NO_THROW(e.db_open(e.m_db_filename));
    EXPECT_TRUE(e.m_db != nullptr);
    EXPECT_TRUE(std::filesystem::exists(e.m_db_filename));

    ASSERT_NO_THROW(e.db_create_schema());

    ASSERT_TRUE(e.add_account("Citibank"));

    auto names = std::set<std::string>{};
    char* errmsg {};
    auto ret = sqlite3_exec(e.m_db, "SELECT name FROM expenses_account;", get_table_names_cb, &names, &errmsg);
    ASSERT_EQ(ret, SQLITE_OK);

    ASSERT_EQ(names.size(), 1);
    EXPECT_TRUE(names.contains("Citibank"));
}

TEST(ExpensesLibTest, add_category) {
    std::filesystem::remove(ExpensesTester::m_db_filename);

    auto e = ExpensesTester(ExpensesTester::DoNothing::DoNothing);
    ASSERT_NO_THROW(e.db_open(e.m_db_filename));
    EXPECT_TRUE(e.m_db != nullptr);
    EXPECT_TRUE(std::filesystem::exists(e.m_db_filename));

    ASSERT_NO_THROW(e.db_create_schema());

    ASSERT_TRUE(e.add_category("Groceries"));

    auto names = std::set<std::string>{};
    char* errmsg {};
    auto ret = sqlite3_exec(e.m_db, "SELECT name FROM expenses_category;", get_table_names_cb, &names, &errmsg);
    ASSERT_EQ(ret, SQLITE_OK);

    ASSERT_EQ(names.size(), 1);
    EXPECT_TRUE(names.contains("Groceries"));
}

TEST(ExpensesLibTest, add_payee) {
    std::filesystem::remove(ExpensesTester::m_db_filename);

    auto e = ExpensesTester(ExpensesTester::DoNothing::DoNothing);
    ASSERT_NO_THROW(e.db_open(e.m_db_filename));
    EXPECT_TRUE(e.m_db != nullptr);
    EXPECT_TRUE(std::filesystem::exists(e.m_db_filename));

    ASSERT_NO_THROW(e.db_create_schema());

    ASSERT_TRUE(e.add_payee("Raleys"));

    auto names = std::set<std::string>{};
    char* errmsg {};
    auto ret = sqlite3_exec(e.m_db, "SELECT name FROM expenses_payee;", get_table_names_cb, &names, &errmsg);
    ASSERT_EQ(ret, SQLITE_OK);

    ASSERT_EQ(names.size(), 1);
    EXPECT_TRUE(names.contains("Raleys"));
}

TEST(ExpensesLibTest, add_payment) {
    std::filesystem::remove(ExpensesTester::m_db_filename);

    auto e = ExpensesTester(ExpensesTester::DoNothing::DoNothing);
    ASSERT_NO_THROW(e.db_open(e.m_db_filename));
    EXPECT_TRUE(e.m_db != nullptr);
    EXPECT_TRUE(std::filesystem::exists(e.m_db_filename));

    ASSERT_NO_THROW(e.db_create_schema());

    std::vector<std::string> accounts {{"CapitalOne"}};
    std::vector<std::string> categories {{"Travel"}};
    std::vector<std::string> payees {{"Southwest Airlines"}};
    ASSERT_TRUE(insert_names(e.m_db, "expenses_account", accounts));
    ASSERT_TRUE(insert_names(e.m_db, "expenses_category", categories));
    ASSERT_TRUE(insert_names(e.m_db, "expenses_payee", payees));
    auto p = Expenses::Payment {.date = 1000, .amount_cents = 500, .account_id = 1, .category_id = 1, .payee_id = 1};

    auto payment_id = e.add_payment(p);
    ASSERT_EQ(payment_id, 1);

    const char* sql = "SELECT ep.paid_date, ep.amount_cents, a.name, c.name, p.name FROM expenses_payment AS ep"
        "     JOIN expenses_account AS a ON a.id = ep.account"
        "     JOIN expenses_category AS c ON c.id = ep.category"
        "     JOIN expenses_payee AS p ON p.id = ep.payee"
        " WHERE ep.id = 1;";
    sqlite3_stmt* stmt {};

    auto ret = sqlite3_prepare_v2(e.m_db, sql, -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        std::cerr << "Error preparing get payment SQLstatement. err=" << ret << ", "
                  << std::quoted(sqlite3_errmsg(e.m_db)) << "\n";
    }
    ASSERT_EQ(ret, SQLITE_OK);

    ret = sqlite3_step(stmt);
    if (ret != SQLITE_ROW) {
        std::cerr << "Error:  First step() should return a row with the payment id. err=" << ret << ", "
                  << std::quoted(sqlite3_errmsg(e.m_db)) << "\n";
    }
    ASSERT_EQ(ret, SQLITE_ROW);

    // Note that pointers returned by column_* functions are
    // invalidated after _step() or _finalize(), so we need to copy
    // them here.
    auto date = sqlite3_column_int(stmt, 0);
    auto amount_cents = sqlite3_column_int(stmt, 1);
    auto account_name = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
    auto category_name = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
    auto payee_name = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));

    ret = sqlite3_step(stmt);
    if (ret != SQLITE_DONE) {
        std::cerr << "Error:  Second step() should return DONE since we only query one row. err=" << ret << ", "
                  << std::quoted(sqlite3_errmsg(e.m_db)) << "\n";
    }
    ASSERT_EQ(ret, SQLITE_DONE);

    ASSERT_EQ(sqlite3_finalize(stmt), SQLITE_OK);

    EXPECT_EQ(date, 1000);
    EXPECT_EQ(amount_cents, 500);
    EXPECT_STREQ(account_name.c_str(), "CapitalOne");
    EXPECT_STREQ(category_name.c_str(), "Travel");
    EXPECT_STREQ(payee_name.c_str(), "Southwest Airlines");
}
