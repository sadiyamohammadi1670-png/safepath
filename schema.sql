-- ============================================================
--  SafePath — Women's Safety Route Finder
--  Database: SQLite / MySQL / PostgreSQL compatible
--  File    : schema.sql
--  Tables  : cities, nodes, edges, incidents, routes, users
-- ============================================================

-- ── Drop tables (for fresh setup) ──────────────────────────
DROP TABLE IF EXISTS route_paths;
DROP TABLE IF EXISTS routes;
DROP TABLE IF EXISTS incidents;
DROP TABLE IF EXISTS edges;
DROP TABLE IF EXISTS nodes;
DROP TABLE IF EXISTS cities;
DROP TABLE IF EXISTS users;

-- ============================================================
--  TABLE: cities
--  Stores city metadata
-- ============================================================
CREATE TABLE cities (
    city_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    city_code   VARCHAR(20)  NOT NULL UNIQUE,   -- e.g. 'delhi', 'mumbai'
    city_name   VARCHAR(100) NOT NULL,
    state       VARCHAR(100),
    country     VARCHAR(60)  DEFAULT 'India',
    created_at  TIMESTAMP    DEFAULT CURRENT_TIMESTAMP
);

-- ============================================================
--  TABLE: nodes
--  Locations / intersections in a city
-- ============================================================
CREATE TABLE nodes (
    node_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    city_id     INTEGER      NOT NULL REFERENCES cities(city_id),
    node_code   VARCHAR(8)   NOT NULL,           -- single-letter ID: A, B ...
    node_name   VARCHAR(100) NOT NULL,           -- human-readable name
    zone_type   VARCHAR(40),                     -- commercial, residential, …
    latitude    REAL,
    longitude   REAL,
    safety_base INTEGER      DEFAULT 7 CHECK(safety_base BETWEEN 1 AND 10),
    UNIQUE(city_id, node_code)
);

-- ============================================================
--  TABLE: edges
--  Roads between pairs of nodes, with distance & safety score
-- ============================================================
CREATE TABLE edges (
    edge_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    city_id       INTEGER NOT NULL REFERENCES cities(city_id),
    node_u        INTEGER NOT NULL REFERENCES nodes(node_id),
    node_v        INTEGER NOT NULL REFERENCES nodes(node_id),
    dist_km       REAL    NOT NULL CHECK(dist_km > 0),
    safety_score  INTEGER NOT NULL CHECK(safety_score BETWEEN 1 AND 10),
    -- Safety feature flags
    has_cctv      INTEGER DEFAULT 0,   -- 1 = yes, 0 = no
    has_lighting  INTEGER DEFAULT 0,
    is_patrol     INTEGER DEFAULT 0,
    is_crowded    INTEGER DEFAULT 0,
    last_updated  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- ============================================================
--  TABLE: incidents
--  Reported safety incidents near nodes
-- ============================================================
CREATE TABLE incidents (
    incident_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    city_id       INTEGER      NOT NULL REFERENCES cities(city_id),
    node_id       INTEGER      NOT NULL REFERENCES nodes(node_id),
    severity      VARCHAR(10)  NOT NULL CHECK(severity IN ('low','medium','high')),
    description   TEXT         NOT NULL,
    reported_at   TIMESTAMP    DEFAULT CURRENT_TIMESTAMP,
    is_active     INTEGER      DEFAULT 1
);

-- ============================================================
--  TABLE: users
--  Registered users of the system
-- ============================================================
CREATE TABLE users (
    user_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    name        VARCHAR(100) NOT NULL,
    email       VARCHAR(150) NOT NULL UNIQUE,
    phone       VARCHAR(20),
    created_at  TIMESTAMP    DEFAULT CURRENT_TIMESTAMP
);

-- ============================================================
--  TABLE: routes
--  Saved route searches by users
-- ============================================================
CREATE TABLE routes (
    route_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id       INTEGER    REFERENCES users(user_id),
    city_id       INTEGER    NOT NULL REFERENCES cities(city_id),
    start_node    INTEGER    NOT NULL REFERENCES nodes(node_id),
    end_node      INTEGER    NOT NULL REFERENCES nodes(node_id),
    route_type    VARCHAR(20) DEFAULT 'safest',  -- safest / fastest / balanced
    total_dist_km REAL,
    avg_safety    REAL,
    weighted_cost REAL,
    computed_at   TIMESTAMP  DEFAULT CURRENT_TIMESTAMP
);

-- ============================================================
--  TABLE: route_paths
--  Individual steps of a saved route
-- ============================================================
CREATE TABLE route_paths (
    path_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    route_id    INTEGER NOT NULL REFERENCES routes(route_id),
    step_order  INTEGER NOT NULL,
    node_id     INTEGER NOT NULL REFERENCES nodes(node_id)
);

-- ============================================================
--  INDEXES for performance
-- ============================================================
CREATE INDEX idx_edges_city     ON edges(city_id);
CREATE INDEX idx_nodes_city     ON nodes(city_id);
CREATE INDEX idx_incidents_city ON incidents(city_id);
CREATE INDEX idx_incidents_node ON incidents(node_id);
CREATE INDEX idx_routes_user    ON routes(user_id);

-- ============================================================
--  SEED DATA — Cities
-- ============================================================
INSERT INTO cities (city_code, city_name, state) VALUES
    ('delhi',     'New Delhi',  'Delhi'),
    ('mumbai',    'Mumbai',     'Maharashtra'),
    ('bangalore', 'Bangalore',  'Karnataka'),
    ('chennai',   'Chennai',    'Tamil Nadu'),
    ('kolkata',   'Kolkata',    'West Bengal'),
    ('hyderabad', 'Hyderabad',  'Telangana');

-- ============================================================
--  SEED DATA — Nodes (New Delhi only shown; replicate for others)
-- ============================================================
INSERT INTO nodes (city_id, node_code, node_name, zone_type, latitude, longitude, safety_base) VALUES
    (1, 'A', 'Connaught Place',  'commercial',  28.6315,  77.2167, 8),
    (1, 'B', 'Karol Bagh',       'residential', 28.6508,  77.1904, 6),
    (1, 'C', 'Lajpat Nagar',     'market',      28.5672,  77.2432, 6),
    (1, 'D', 'Hauz Khas',        'upscale',     28.5494,  77.2001, 9),
    (1, 'E', 'Saket',            'mall',        28.5274,  77.2167, 8),
    (1, 'F', 'Vasant Kunj',      'residential', 28.5213,  77.1601, 7),
    (1, 'G', 'Dwarka',           'suburb',      28.5921,  77.0460, 5),
    (1, 'H', 'RK Puram',         'govt',        28.5651,  77.1750, 8);

-- Mumbai nodes
INSERT INTO nodes (city_id, node_code, node_name, zone_type, latitude, longitude, safety_base) VALUES
    (2, 'A', 'CST',          'transport',   18.9402,  72.8356, 8),
    (2, 'B', 'Dadar',        'market',      19.0178,  72.8478, 7),
    (2, 'C', 'Bandra',       'upscale',     19.0596,  72.8295, 8),
    (2, 'D', 'Andheri',      'commercial',  19.1136,  72.8697, 7),
    (2, 'E', 'Powai',        'residential', 19.1197,  72.9050, 7),
    (2, 'F', 'Juhu',         'beach',       19.1075,  72.8263, 6),
    (2, 'G', 'Borivali',     'suburb',      19.2288,  72.8567, 6),
    (2, 'H', 'Thane',        'suburb',      19.2183,  72.9781, 7);

-- ============================================================
--  SEED DATA — Edges (New Delhi)
-- ============================================================
INSERT INTO edges (city_id, node_u, node_v, dist_km, safety_score, has_cctv, has_lighting, is_patrol, is_crowded)
VALUES
    (1, 1, 2, 4.2, 8,  1, 1, 1, 1),
    (1, 1, 3, 5.5, 7,  1, 1, 0, 1),
    (1, 1, 4, 7.0, 9,  1, 1, 1, 0),
    (1, 2, 7, 8.1, 5,  0, 0, 0, 1),
    (1, 2, 4, 5.8, 7,  1, 1, 0, 1),
    (1, 3, 5, 4.3, 6,  0, 1, 0, 1),
    (1, 4, 5, 3.1, 9,  1, 1, 1, 0),
    (1, 4, 8, 2.8, 8,  1, 1, 1, 0),
    (1, 5, 6, 5.2, 7,  1, 1, 0, 0),
    (1, 5, 8, 2.5, 8,  1, 1, 1, 0),
    (1, 6, 7, 6.3, 5,  0, 0, 0, 0),
    (1, 6, 8, 3.8, 8,  1, 1, 0, 0),
    (1, 7, 8, 7.2, 4,  0, 0, 0, 0);

-- ============================================================
--  SEED DATA — Incidents
-- ============================================================
INSERT INTO incidents (city_id, node_id, severity, description) VALUES
    (1, 7, 'high',   'Poorly lit stretch after 9pm — avoid solo travel'),
    (1, 2, 'medium', 'Crowded market area — pickpocket risk near bus stand'),
    (1, 3, 'medium', 'Limited CCTV coverage on connecting lane to market'),
    (2, 6, 'medium', 'Beach area — limit solo walking at night'),
    (2, 7, 'medium', 'Isolated stretches near forest boundary');

-- ============================================================
--  SEED DATA — Sample user
-- ============================================================
INSERT INTO users (name, email, phone) VALUES
    ('Priya Sharma', 'priya@example.com', '+91-9876543210'),
    ('Anita Singh',  'anita@example.com', '+91-9123456780');

-- ============================================================
--  USEFUL QUERIES
-- ============================================================

-- 1. List all roads in Delhi sorted by safety (DESC) — mirrors Quick Sort in C
SELECT
    n1.node_name AS "From",
    n2.node_name AS "To",
    e.dist_km    AS "Distance (km)",
    e.safety_score AS "Safety Score",
    CASE WHEN e.has_cctv=1     THEN '✔' ELSE '✘' END AS CCTV,
    CASE WHEN e.has_lighting=1 THEN '✔' ELSE '✘' END AS Lighting
FROM   edges e
JOIN   nodes n1 ON e.node_u = n1.node_id
JOIN   nodes n2 ON e.node_v = n2.node_id
WHERE  e.city_id = 1
ORDER  BY e.safety_score DESC;

-- 2. Active incidents with severity
SELECT
    c.city_name,
    n.node_name,
    i.severity,
    i.description,
    i.reported_at
FROM   incidents i
JOIN   nodes  n ON i.node_id  = n.node_id
JOIN   cities c ON i.city_id  = c.city_id
WHERE  i.is_active = 1
ORDER  BY CASE i.severity WHEN 'high' THEN 1 WHEN 'medium' THEN 2 ELSE 3 END;

-- 3. Average safety per city
SELECT
    c.city_name,
    ROUND(AVG(e.safety_score), 2) AS avg_safety,
    COUNT(*)                       AS num_roads,
    MIN(e.safety_score)            AS min_safety,
    MAX(e.safety_score)            AS max_safety
FROM   edges e
JOIN   cities c ON e.city_id = c.city_id
GROUP  BY c.city_id
ORDER  BY avg_safety DESC;

-- 4. Most dangerous routes (safety < 6)
SELECT
    n1.node_name AS "From",
    n2.node_name AS "To",
    e.dist_km,
    e.safety_score AS danger_level
FROM   edges e
JOIN   nodes n1 ON e.node_u = n1.node_id
JOIN   nodes n2 ON e.node_v = n2.node_id
WHERE  e.safety_score < 6
ORDER  BY e.safety_score ASC;

-- 5. Save a computed route (example INSERT)
INSERT INTO routes (user_id, city_id, start_node, end_node, route_type, total_dist_km, avg_safety, weighted_cost)
VALUES (1, 1, 1, 6, 'safest', 13.1, 8.5, 28.7);

-- 6. Most searched routes
SELECT
    n1.node_name AS start,
    n2.node_name AS destination,
    COUNT(*)     AS times_searched,
    AVG(avg_safety) AS avg_route_safety
FROM   routes r
JOIN   nodes n1 ON r.start_node = n1.node_id
JOIN   nodes n2 ON r.end_node   = n2.node_id
GROUP  BY r.start_node, r.end_node
ORDER  BY times_searched DESC;
