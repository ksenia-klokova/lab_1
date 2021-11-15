База данных создается, индексируется и заполняется при запуске ./start.sh

# Примеры вывода для запросов

### GET

Первый запрос:

http://192.168.243.128/person?login=Carmella_Hamilton1725@sheye.org

```
{"age":13,"first_name":"Carmella","last_name":"Hamilton","login":"Carmella_Hamilton1725@sheye.org"}
```
```
cache missed for login:Carmella_Hamilton1725@sheye.org
saved to cache:Carmella_Hamilton1725@sheye.org
cache size:1
```

Второй запрос (с кэшем):
```
item from cache:Carmella_Hamilton1725@sheye.org
```

http://192.168.243.128/person?
```
Unknown request
```

http://192.168.243.128/person?search&first_name=Marigold&last_name=Driscoll
```
[{"age":81,"first_name":"Marigold","last_name":"Driscoll","login":"Marigold_Driscoll4284@jiman.org"}]
```

### POST

http://192.168.243.128/person?add&first_name=Paul&last_name=Dano&age=37&login=pauldano@hollywood.com

```
inserted:pauldano@hollywood.com
saved to cache:pauldano@hollywood.com
cache size:2
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
