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