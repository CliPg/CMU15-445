SELECT release_info.date_year || '-' || release_info.date_month || '-' || release_info.date_day AS RELEASE_DATE, 
release.name AS RELEASE_NAME, 
artist_credit.artist_count AS ARTIST_COUNT
FROM release_info
JOIN release ON release_info.release = release.id
JOIN artist_credit ON release.artist_credit = artist_credit.id
WHERE artist_credit.artist_count >= 2
    AND release_info.date_year IS NOT NULL
    AND release_info.date_month IS NOT NULL
    AND release_info.date_day IS NOT NULL
ORDER BY release_info.date_year DESC, 
         release_info.date_month DESC, 
         release_info.date_day DESC,
         release.name ASC
LIMIT 10;