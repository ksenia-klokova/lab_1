#include "person.h"
#include "database.h"
#include "../config/config.h"

#include <Poco/Data/MySQL/Connector.h>
#include <Poco/Data/MySQL/MySQLException.h>
#include <Poco/Data/SessionFactory.h>
#include <Poco/Data/RecordSet.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Dynamic/Var.h>

#include <sstream>
#include <exception>
#include <fstream>
#include <algorithm>
#include <future>

using namespace Poco::Data::Keywords;
using Poco::Data::Session;
using Poco::Data::Statement;

namespace database
{
    void Person::init()
    {
        try
        {

            Poco::Data::Session session = database::Database::get().create_session();
            
            Statement drop_stmt(session);
            drop_stmt << "DROP TABLE IF EXISTS Person", now;
            //

            // (re)create table
            Statement create_stmt(session);
            create_stmt << "CREATE TABLE IF NOT EXISTS `Person` (`id` INT NOT NULL AUTO_INCREMENT,"
                        << "`login` VARCHAR(256) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL,"
                        << "`first_name` VARCHAR(256) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL,"
                        << "`last_name` VARCHAR(256) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL,"
                        << "`age` TINYINT NULL,"
                        << "PRIMARY KEY (`id`));",
                now;
            std::cout << "table created" << std::endl;

            // populate table
            std::ifstream file("database/person_data.json");
            std::stringstream tmp;
            tmp << file.rdbuf();

            Poco::JSON::Parser parser;
            Poco::Dynamic::Var result = parser.parse(tmp.str());
            Poco::JSON::Object::Ptr obj = result.extract<Poco::JSON::Object::Ptr>();
            Poco::JSON::Array::Ptr arr = obj->getArray("objects");

            size_t i{0};
            for (i = 0; i < arr->size(); ++i)
            {
                Poco::JSON::Object::Ptr object = arr->getObject(i);
                std::string login = object->getValue<std::string>("login");
                std::string first_name = object->getValue<std::string>("first_name");
                std::string last_name = object->getValue<std::string>("last_name");
                int age = object->getValue<int>("age");

                Poco::Data::Statement insert(session);
                std::string sharding_hint = database::Database::sharding_hint(login);
                
                std::string select_str = "INSERT INTO Person (login, first_name, last_name, age) VALUES(?, ?, ?, ?)";
                select_str += sharding_hint;

                insert << select_str,
                    Poco::Data::Keywords::use(login),
                    Poco::Data::Keywords::use(first_name),
                    Poco::Data::Keywords::use(last_name),
                    Poco::Data::Keywords::use(age);

                insert.execute();
                if (i % 5000 == 0)
                {
                    std::cout << "Inserted " << i << " records" << std::endl;
                }
            }

        std::cout << "All done. Inserted " << i << " records" << std::endl; 

        // Create indices
        Statement login_idx(session);
        login_idx << "CREATE INDEX lg USING HASH ON Person(login);", now;

        Statement fn_ln_idx(session);
        fn_ln_idx << "CREATE INDEX fn_ln USING BTREE ON Person(first_name,last_name);", now;

        std::cout << "table indexed" << std::endl;
        }

        catch (Poco::Data::MySQL::ConnectionException &e)
        {
            std::cout << "connection:" << e.what() << std::endl;
            throw;
        }
        catch (Poco::Data::MySQL::StatementException &e)
        {

            std::cout << "statement:" << e.what() << std::endl;
            throw;
        }
    }

    Poco::JSON::Object::Ptr Person::toJSON() const
    {
        Poco::JSON::Object::Ptr root = new Poco::JSON::Object();

        root->set("login", _login);
        root->set("first_name", _first_name);
        root->set("last_name", _last_name);
        root->set("age", _age);

        return root;
    }

    Person Person::fromJSON(const std::string &str)
    {
        Person person;
        Poco::JSON::Parser parser;
        Poco::Dynamic::Var result = parser.parse(str);
        Poco::JSON::Object::Ptr object = result.extract<Poco::JSON::Object::Ptr>();

        person.id() = object->getValue<long>("id");
        person.login() = object->getValue<std::string>("login");
        person.first_name() = object->getValue<std::string>("first_name");
        person.last_name() = object->getValue<std::string>("last_name");
        person.age() = object->getValue<int>("age");

        return person;
    }

    Person Person::read_by_login(std::string login)
    {
        try
        {
            Poco::Data::Session session = database::Database::get().create_session();
            Poco::Data::Statement select(session);
            Person p;
            std::string sharding_hint = database::Database::sharding_hint(login);
            std::string select_str = "SELECT login, first_name, last_name, age FROM Person where login=?";
            select_str += sharding_hint;
            
            select << select_str,
                into(p._login),
                into(p._first_name),
                into(p._last_name),
                into(p._age),
                use(login),
                range(0, 1); //  iterate over result set one row at a time
            select.execute();
            Poco::Data::RecordSet rs(select);

            std::cout << "Selected " << p._login << " from shard " << sharding_hint << std::endl;

            if (!rs.moveFirst()) throw std::logic_error("not found");

            return p;
        }

        catch (Poco::Data::MySQL::ConnectionException &e)
        {
            std::cout << "connection:" << e.what() << std::endl;
            throw;
        }
        catch (Poco::Data::MySQL::StatementException &e)
        {

            std::cout << "statement:" << e.what() << std::endl;
            throw;
        }
    }

    std::vector<Person> Person::search(std::string first_name, std::string last_name)
    {
        std::vector<Person> result;
        // get all hints for shards
        std::vector<std::string> hints = database::Database::get_all_hints();
        std::vector<std::future<std::vector<Person>>> futures;

        first_name = "\"" + first_name + "\"";
        last_name = "\"" + last_name + "\"";

        // map phase in parallel
        for (const std::string &hint : hints)
        {
            auto handle = std::async(std::launch::async, [first_name, last_name, hint]() -> std::vector<Person>
            {
                try
                {
                    std::vector<Person> result;
                    Poco::Data::Session session = database::Database::get().create_session();
                    Statement select(session);

                    Person p;
                    
                    std::string select_str = "SELECT login, first_name, last_name, age FROM Person where first_name LIKE ";
                    select_str += first_name;
                    select_str += " AND last_name LIKE ";
                    select_str += last_name;
                    
                    select_str += hint;

                    //std::cout << select_str << std::endl;

                    select << select_str,
                        into(p._login),
                        into(p._first_name),
                        into(p._last_name),
                        into(p._age),
                        range(0, 1); //  iterate over result set one row at a time

                    while (!select.done())
                    {
                        select.execute();
                        result.push_back(p);
                    }

                    std::cout << "Selected " << p._first_name << p._last_name << " from shard " << hint << std::endl;

                    return result;
                }

                catch (Poco::Data::MySQL::ConnectionException &e)
                {
                    std::cout << "connection:" << e.what() << std::endl;
                    throw;
                }
                catch (Poco::Data::MySQL::StatementException &e)
                {

                    std::cout << "statement:" << e.what() << std::endl;
                    throw;
                }
            });
            futures.emplace_back(std::move(handle));
        }
        // reduce phase
        // get values
        for(std::future<std::vector<Person>>& res : futures){
            std::vector<Person> v= res.get();
            std::copy(std::begin(v),
                      std::end(v),
                      std::back_inserter(result));
        }

        return result;
    }

    void Person::save_to_mysql()
    {
        try
        {
            Poco::Data::Session session = database::Database::get().create_session();
            Poco::Data::Statement insert(session);
            std::string sharding_hint = database::Database::sharding_hint(_login);

            std::string select_str = "INSERT INTO Person (login, first_name, last_name, age) VALUES(?, ?, ?, ?)";
            select_str += sharding_hint;

            insert << select_str,
                use(_login),
                use(_first_name),
                use(_last_name),
                use(_age);

            insert.execute();

            Poco::Data::Statement select(session);
            select << "SELECT login FROM Person ORDER BY id DESC LIMIT 1" + sharding_hint,
                into(_login),
                range(0, 1); //  iterate over result set one row at a time

            if (!select.done())
            {
                select.execute();
            }
            std::cout << "inserted " << _login << " to shard " << sharding_hint << std::endl;
            //}
        }
        catch (Poco::Data::MySQL::ConnectionException &e)
        {
            std::cout << "connection:" << e.what() << std::endl;
            throw;
        }
        catch (Poco::Data::MySQL::StatementException &e)
        {

            std::cout << "statement:" << e.what() << std::endl;
            throw;
        }
    }

    long Person::get_id() const
    {
        return _id;
    }

    const std::string &Person::get_login() const
    {
        return _login;
    }

    const std::string &Person::get_first_name() const
    {
        return _first_name;
    }

    const std::string &Person::get_last_name() const
    {
        return _last_name;
    }

    int Person::get_age() const
    {
        return _age;
    }

    long &Person::id()
    {
        return _id;
    }

    std::string &Person::login()
    {
        return _login;
    }

    std::string &Person::first_name()
    {
        return _first_name;
    }

    std::string &Person::last_name()
    {
        return _last_name;
    }

    int &Person::age()
    {
        return _age;
    }
}