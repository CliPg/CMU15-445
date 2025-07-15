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
