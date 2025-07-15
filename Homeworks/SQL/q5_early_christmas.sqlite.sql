WITH
ranked_artist AS (
  SELECT
    artist.id AS a_i,
    artist.name AS a_n,
    artist_credit.id AS ac_i,
    COUNT(DISTINCT release.id) AS r_c
  FROM release
  JOIN artist_credit ON release.artist_credit = artist_credit.id
  JOIN artist_credit_name ON artist_credit.id = artist_credit_name.artist_credit
  JOIN artist ON artist_credit_name.artist = artist.id
  JOIN release_info ON release.id = release_info.release
  WHERE LOWER(release.name) LIKE '%christmas%'
    AND artist.type = 1
  GROUP BY artist.id, artist.name, artist_credit.id
),
c_artist AS (
  SELECT *
  FROM ranked_artist
  ORDER BY r_c DESC, a_n
  LIMIT 11
),
dated_release AS (
  SELECT
    release.id AS r_id,
    release.name AS r_name,
    acn.artist AS artist_id,
    ri.date_year,
    ri.date_month,
    ri.date_day
  FROM release
  JOIN artist_credit AS ac ON release.artist_credit = ac.id
  JOIN artist_credit_name AS acn ON ac.id = acn.artist_credit
  JOIN release_info AS ri ON release.id = ri.release
  WHERE ri.date_month = 11
    AND ri.date_year IS NOT NULL
    AND ri.date_day IS NOT NULL
  GROUP BY acn.artist, release.name, ri.date_year, ri.date_month, ri.date_day
),
joined AS (
  SELECT
    c.a_n AS artist_name,
    d.r_name AS release_name,
    d.date_year,
    d.date_month,
    d.date_day,
    c.r_c AS song_count
  FROM c_artist c
  JOIN dated_release d ON c.a_i = d.artist_id
),
numbered AS (
  SELECT *,
         (SELECT COUNT(*) FROM joined j2
          WHERE j2.artist_name = j1.artist_name
            AND (
              j2.date_year < j1.date_year OR
              (j2.date_year = j1.date_year AND j2.date_month < j1.date_month) OR
              (j2.date_year = j1.date_year AND j2.date_month = j1.date_month AND j2.date_day < j1.date_day) OR
              (j2.date_year = j1.date_year AND j2.date_month = j1.date_month AND j2.date_day = j1.date_day AND j2.release_name < j1.release_name)
            )
         ) + 1 AS rk
  FROM joined j1
)

SELECT
  artist_name AS ARTIST_NAME,
  release_name AS RELEASE_NAME,
  date_year || '-' || date_month || '-' || date_day AS RELEASE_DATE
FROM numbered
WHERE rk <= 5
ORDER BY song_count DESC, artist_name, release_name, date_year, date_month, date_day;
