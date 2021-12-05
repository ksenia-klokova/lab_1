База данных создается, индексируется и заполняется при запуске ./start.sh

# Примеры вывода для запросов

### GET

http://192.168.95.128/person?login=Carmella_Hamilton1725@sheye.org

```
{"age":13,"first_name":"Carmella","last_name":"Hamilton","login":"Carmella_Hamilton1725@sheye.org"}
```
```
Selected Carmella_Hamilton1725@sheye.org from shard -- sharding:1
```

http://192.168.95.128/person?
```
Unknown request
```

http://192.168.95.128/person?search&first_name=Marigold&last_name=Driscoll
```
[{"age":81,"first_name":"Marigold","last_name":"Driscoll","login":"Marigold_Driscoll4284@jiman.org"},
{"age":81,"first_name":"Marigold","last_name":"Driscoll","login":"Marigold_Driscoll4284@jiman.org"},
{"age":81,"first_name":"Marigold","last_name":"Driscoll","login":"Marigold_Driscoll4284@jiman.org"}]
```
```
Selected MarigoldDriscoll from shard -- sharding:2
Selected MarigoldDriscoll from shard -- sharding:1
Selected MarigoldDriscoll from shard -- sharding:0
```

### POST

http://192.168.95.128/person?add&first_name=Paul&last_name=Dano&age=37&login=pauldano@hollywood.com

```
inserted pauldano1@hollywood.com to shard -- sharding:1
```

```
mysql> select * from Person where first_name="Paul";
+-------+------------------------+------------+-----------+------+
| id    | login                  | first_name | last_name | age  |
+-------+------------------------+------------+-----------+------+
| 50001 | pauldano@hollywood.com | Paul       | Dano      |   37 |
+-------+------------------------+------------+-----------+------+
```

Если попробуем добавить этого пользователя:

 ```
User with login pauldano@hollywood.com already exists
```
