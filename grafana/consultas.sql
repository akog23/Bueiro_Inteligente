-- ============================================================
-- Bueiro Inteligente - consultas dos painéis do Grafana
-- Data source: InfluxDB (InfluxDB Cloud Serverless), linguagem SQL
-- Database (bucket): bueiro_inteligente | Measurement: leituras
-- ============================================================


-- ------------------------------------------------------------
-- Painel 1: Visão geral dos bueiros
-- Visualização: Stat | Text mode: Value and name | Color mode: Background solid
-- Overrides de cor: Total de bueiros = azul, Bueiros OK = verde,
--                   Atenção = amarelo, Críticos = vermelho
-- ------------------------------------------------------------
WITH ultimas AS (
  SELECT id_bueiro, status,
         ROW_NUMBER() OVER (PARTITION BY id_bueiro ORDER BY time DESC) AS ordem
  FROM "leituras"
  WHERE time >= $__timeFrom AND time <= $__timeTo
)
SELECT
  COUNT(*) AS "Total de bueiros",
  SUM(CASE WHEN status = 'verde'    THEN 1 ELSE 0 END) AS "Bueiros OK",
  SUM(CASE WHEN status = 'amarelo'  THEN 1 ELSE 0 END) AS "Atenção",
  SUM(CASE WHEN status = 'vermelho' THEN 1 ELSE 0 END) AS "Críticos"
FROM ultimas
WHERE ordem = 1;


-- ------------------------------------------------------------
-- Painel 2: Sistema online (segundos desde a última leitura)
-- Visualização: Stat | Unit: seconds (s)
-- Thresholds: Base = verde, 30 = vermelho
-- ------------------------------------------------------------
SELECT (CAST(now() AS BIGINT) - CAST(MAX(time) AS BIGINT)) / 1000000000 AS "Última leitura há"
FROM "leituras"
WHERE time >= $__timeFrom AND time <= $__timeTo;


-- ------------------------------------------------------------
-- Painel 3: Mapa dos bueiros
-- Visualização: Geomap | Map view: Fit to data
-- Layer: Markers | Color: campo "Obstrução (%)"
-- Thresholds: Base = verde, 50 = amarelo, 75 = vermelho
-- ------------------------------------------------------------
WITH ultimas AS (
  SELECT id_bueiro, endereco, latitude, longitude, obstrucao_pct, status,
         ROW_NUMBER() OVER (PARTITION BY id_bueiro ORDER BY time DESC) AS ordem
  FROM "leituras"
  WHERE time >= $__timeFrom AND time <= $__timeTo
    AND latitude IS NOT NULL
)
SELECT id_bueiro     AS "Bueiro",
       endereco      AS "Endereço",
       latitude,
       longitude,
       obstrucao_pct AS "Obstrução (%)",
       status        AS "Status"
FROM ultimas
WHERE ordem = 1;


-- ------------------------------------------------------------
-- Painel 4: Últimos alertas (mudanças de estado para atenção/crítico)
-- Visualização: Table
-- Value mappings na coluna "Nível": amarelo -> ATENÇÃO (amarelo),
--                                   vermelho -> CRÍTICO (vermelho)
-- Override "Nível": Cell type = Colored background
-- ------------------------------------------------------------
WITH historico AS (
  SELECT time, id_bueiro, endereco, status, obstrucao_pct,
         LAG(status) OVER (PARTITION BY id_bueiro ORDER BY time) AS status_anterior
  FROM "leituras"
  WHERE time >= $__timeFrom AND time <= $__timeTo
)
SELECT time          AS "Horário",
       id_bueiro     AS "Bueiro",
       endereco      AS "Endereço",
       status        AS "Nível",
       obstrucao_pct AS "Resíduo (%)"
FROM historico
WHERE status IN ('amarelo', 'vermelho')
  AND (status_anterior IS NULL OR status_anterior <> status)
ORDER BY time DESC
LIMIT 10;


-- ------------------------------------------------------------
-- Painel 5: Evolução da obstrução
-- Visualização: Time series | Unit: Percent (0-100) | Min 0, Max 100
-- Thresholds: Base = verde, 50 = amarelo, 75 = vermelho
-- Show thresholds: As filled regions and lines
-- ------------------------------------------------------------
SELECT time, obstrucao_pct AS "Obstrução (%)"
FROM "leituras"
WHERE time >= $__timeFrom AND time <= $__timeTo
ORDER BY time;
