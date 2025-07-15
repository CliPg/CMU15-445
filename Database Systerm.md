# Database Systerm

## 01 Relational Model & Algebra

### 1. 数据库基础概念

数据库 vs 数据库管理系统(DBMS)

- **数据库**：组织化的相关数据集合，用于建模现实世界的某些方面
- **DBMS**：管理数据库的软件(如MySQL, Oracle)，负责数据的插入、删除和检索

#### 平面文件(Flat File)的问题

- **数据完整性**：难以确保数据一致性(如艺术家与专辑的关联)
- **实现问题**：记录查找困难，多应用/多机器共享问题，并发写入冲突
- **持久性**：崩溃恢复困难，多机器复制困难

### 2. 数据库管理系统(DBMS)概述

**数据模型**

- **关系模型**(最常用)
- **NoSQL**：键值、文档、图数据库
- **数组/矩阵/向量**(机器学习)
- **层次/网络/多值**(已过时)

**模式(Schema)**

基于数据模型描述特定数据集合，定义数据的结构

### 3. 关系模型

Codd提出的三大关键思想

1. 使用简单数据结构(关系)存储数据库
2. 物理存储由DBMS实现决定
3. 通过高级(声明式)语言访问数据，DBMS决定最佳执行策略

关系数据模型三要素

1. **结构**：关系及其内容的定义，独立于物理表示
   - 关系(表)由属性(列)组成
   - 每个属性有值域
2. **完整性**：确保数据库内容满足约束条件
3. **操作**：访问和修改数据库内容的接口(SQL等)

关系术语

- **元组(Tuple)**：关系中一组属性值(表中的行)
- **n元关系**：具有n个属性的关系(即n列表)
- **主键(Primary Key)**：唯一标识表中元组
- **外键(Foreign Key)**：一个关系的属性映射到另一个关系的元组
- **约束(Constraint)**：用户定义的条件(如非空、唯一性等)

### 4. 数据操作语言(DML)

两类语言

1. **过程式**：指定DBMS应使用的高级执行策略
2. **声明式(非过程式)**：只指定所需数据，不指定如何获取(SQL)

### 5. 关系代数

基本操作

1. **选择(Select)**：σ_predicate_(R)
   - 示例：σ_a_id='a2'_(R)
   - SQL等效：`SELECT * FROM R WHERE a_id = 'a2'`
2. **投影(Projection)**：π_A1,A2,...,An_(R)
   - 示例：π_b_id-100,a_id_(σ_a_id='a2'_(R))
   - SQL等效：`SELECT b_id-100, a_id FROM R WHERE a_id = 'a2'`
3. **并集(Union)**：R ∪ S
   - 要求：两关系属性完全相同
   - SQL等效：`(SELECT * FROM R) UNION ALL (SELECT * FROM S)`
4. **交集(Intersection)**：R ∩ S
   - SQL等效：`(SELECT * FROM R) INTERSECT (SELECT * FROM S)`
5. **差集(Difference)**：R - S
   - SQL等效：`(SELECT * FROM R) EXCEPT (SELECT * FROM S)`
6. **乘积(Product)**：R × S
   - SQL等效：`SELECT * FROM R CROSS JOIN S` 或 `SELECT * FROM R, S`
7. **连接(Join)**：R ⋈ S
   - SQL等效：`SELECT * FROM R JOIN S USING (ATTRIBUTE1, ATTRIBUTE2...)`

关系代数优化观察

- 操作顺序影响性能但不影响结果
- 声明式语言(SQL)让DBMS决定最佳执行策略
- 示例：σ_b_id=102_(R ⋈ S) vs R ⋈ (σ_b_id=102_(S)) 结果相同但后者可能更高效

### 6. 其他数据模型

文档数据模型

- 包含层次化字段/值对的文档集合
- 现代实现使用JSON，旧系统使用XML
- 仍面临许多平面文件的问题

向量数据模型

- 表示一维数组，用于最近邻搜索
- 用于基于ML生成的嵌入的语义搜索
- 许多关系DBMS已添加向量索引功能(pgvector)

### 7. 附加内容：跳表(Skip List)

- 概率数据结构，实现有序集合
- 支持插入、删除和搜索，平均对数复杂度
- 类似链表但包含额外指针，可跳过多个节点加速搜索

### 关键要点

1. 关系模型通过抽象物理层解决了早期DBMS的维护问题
2. 关系代数提供了操作关系的基础运算
3. 声明式查询(SQL)让DBMS优化执行策略，是关系模型的强大优势
4. 理解关系模型的基本概念(关系、元组、主键、外键)对数据库设计至关重要

## 02 SQL
SQL是为关系型数据库设计的声明式语言

关系型语言由不同类别的命令组成
- Data Manipulation Language(DML)
- Data Definition Language(DDL)
- Data Control Language(DCL)

eg.
```SQL
CREATE TABLE student (
sid INT PRIMARY KEY,
name VARCHAR(16),
login VARCHAR(32) UNIQUE,
age SMALLINT,
gpa FLOAT
);
CREATE TABLE course (
cid VARCHAR(32) PRIMARY KEY,
name VARCHAR(32) NOT NULL
);
CREATE TABLE enrolled (
sid INT REFERENCES student (sid),
cid VARCHAR(32) REFERENCES course (cid),
grade CHAR(1)
);
```

### 聚合函数
常用聚合函数：AVG, MIN, MAX, SUM, COUNT

支持 DISTINCT 关键字（如 COUNT(DISTINCT login)）。

非聚合列必须出现在 GROUP BY 中。

HAVING 用于过滤分组结果（类似 WHERE 但针对聚合值）。

### 字符串操作
字符串区分大小写，使用单引号。

LIKE 支持通配符：

%：匹配任意子串

_：匹配单个字符

函数示例：SUBSTRING, UPPER，连接符 ||（部分系统可能不同）。

### 输出控制
ORDER BY 排序（默认 ASC，可指定 DESC）。

LIMIT 限制返回行数，OFFSET 跳过行。

未排序时，结果可能因 DBMS 实现不同而变化。

### 窗口函数
对滑动窗口内的数据进行计算，不折叠结果。

函数示例：

ROW_NUMBER()：行号（排序前计算）

RANK()：排名（排序后计算）

语法：OVER (PARTITION BY ... ORDER BY ...)

### 嵌套查询
子查询可出现在 SELECT、FROM、WHERE 等子句中。

常用表达式：

IN / NOT IN：匹配子查询结果

EXISTS / NOT EXISTS：检查子查询是否返回行

ANY / ALL：比较子查询结果

### Lateral Joins
允许子查询引用前置查询的列，类似循环遍历。

### 公用表表达式（CTE）
WITH 创建临时表，简化复杂查询。

递归 CTE（WITH RECURSIVE）支持递归操作，使 SQL 具备图灵完备性。

### 其他特性
日期和时间：支持操作，但语法因系统而异。

输出重定向：将查询结果存入新表或现有表（INTO 或 INSERT INTO）。