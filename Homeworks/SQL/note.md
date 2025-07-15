SQLite

sqlite3 musicbrainz-cmudb2025.db

Duckdb

To launch DuckDB now, type
```
/home/clipg/.duckdb/cli/latest/duckdb
```

查看数据表
```
D .open musicbrainz-cmudb2025.db
```

# 数据库表名
area                artist_credit_name  medium              release_status    
artist              artist_type         medium_format       work              
artist_alias        gender              release             work_type         
artist_credit       language            release_info

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/e594e0c00c5a422c9a2797620a5c44f0.png)

# Q1
select distinct(name) from artist_type order by name;

# Q2
找到所有`artists`,
在`united states`,
出生在`7.4`,
发表过`非英语音乐`

```SQL
CREATE TABLE [artist] (
   [id] INTEGER,
   [name] TEXT,
   [begin_date_year] INTEGER,
   [begin_date_month] INTEGER,
   [begin_date_day] INTEGER,
   [end_date_year] INTEGER,
   [end_date_month] INTEGER,
   [end_date_day] INTEGER,
   [type] INTEGER,
   [area] INTEGER,
   [gender] INTEGER,
   [comment] TEXT
);

CREATE TABLE [medium_format] (
   [id] INTEGER,
   [name] TEXT,
   [description] TEXT
);

CREATE TABLE [artist_credit] (
   [id] INTEGER,
   [name] TEXT,
   [artist_count] INTEGER
);

CREATE TABLE [artist_credit_name] (
   [artist_credit] INTEGER,
   [position] INTEGER,
   [artist] INTEGER,
   [name] TEXT
);

CREATE TABLE [area] (
   [id] INTEGER,
   [name] TEXT,
   [comment] TEXT
);

CREATE TABLE [release] (
   [id] INTEGER,
   [name] TEXT,
   [artist_credit] INTEGER,
   [status] INTEGER,
   [language] INTEGER,
   [comment] TEXT
);

CREATE TABLE [release_info] (
   [release] INTEGER,
   [area] INTEGER,
   [date_year] INTEGER,
   [date_month] INTEGER,
   [date_day] INTEGER
);
```

```SQL
SELECT DISTINCT artist.name
FROM artist
JOIN artist_credit_name ON artist.id = artist_credit_name.artist
JOIN release ON release.artist_credit = artist_credit_name.artist
JOIN release_info ON release_info.release = 
release.id
WHERE artist.area = (SELECT id FROM area WHERE name = 'United States')
  AND artist.begin_date_month = 7
  AND artist.begin_date_day = 4
  AND release.language != 120
ORDER BY artist.name;
```

## 多表连接
在 SQL 中，多表连接（JOIN）是数据库查询的核心技巧之一。它可以让你从多个相关的表中提取数据。

多表连接的本质：
如果两个表之间有“外键”或“某个字段能对应起来”，就可以通过 JOIN 把它们连在一起，组合出一张新表。

基本语法（INNER JOIN）
SELECT 表1.字段名, 表2.字段名
FROM 表1
JOIN 表2
  ON 表1.字段名 = 表2.字段名

例子：
假设你有这两个表：
artist(id, name, area)
area(id, name)

你想查出所有艺术家的名字和他们所在地区的名字。
SELECT artist.name, area.name
FROM artist
JOIN area
  ON artist.area = area.id;
意思是：把 artist 表和 area 表中 area 对应的 ID 连起来，用 JOIN 把信息合并。

多表连接的扩展（多个 JOIN）
你可以链式地连接多个表：

SELECT a.name, b.title, c.format
FROM artist a
JOIN release b ON a.id = b.artist_id
JOIN medium c ON b.medium_id = c.id;

关键点：
语法结构	说明
JOIN	默认是 INNER JOIN，只返回匹配的行
LEFT JOIN	保留左表所有行，右表无匹配则为 NULL
ON	指定连接条件（字段相等）
USING (字段)	如果两个表字段名一样，可以简写为 USING

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/6291a58e78d34323b3d99af6dcf9337c.png)

# Q3
```SQL
SELECT release_info.date_year || '-' || release_info.date_month || '-' || release_info.date_day AS RELEASE_DATE, 
release.name AS RELEASE_NAME, 
artist_credit.artist_count AS ARTIST_COUNT
FROM release_info
JOIN release ON release_info.release = release.id
JOIN artist_credit ON release.artist_credit = artist_credit.id
WHERE release_info.date_year IS NOT NULL
    AND release_info.date_month IS NOT NULL
    AND release_info.date_day IS NOT NULL
ORDER BY release_info.date_year DESC, 
         release_info.date_month DESC, 
         release_info.date_day DESC,
         release.name ASC
LIMIT 10;
```

# Q4
```SQL
SELECT format_name, release_name
FROM (
  SELECT
    medium_format.name AS format_name,
    release.name AS release_name,
    RANK() OVER (
      PARTITION BY medium_format.name
      ORDER BY LENGTH(release.name) DESC, release.name ASC
    ) AS rk
  FROM medium
  JOIN release ON medium.release = release.id
  JOIN medium_format ON medium.format = medium_format.id
  WHERE medium_format.name LIKE '%CD%'
)
WHERE rk = 1
ORDER BY format_name, release_name;
```

SQLite 会提示 no such column，因为子查询结果集中其实没有 medium_format.name，而是匿名列。