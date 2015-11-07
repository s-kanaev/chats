.open /var/chat-p2p-mu/clients.db

BEGIN TRANSACTION;

CREATE TABLE clients (
    id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
    nickname CHAR(24),
    keyword CHAR(40),
    host TEXT,
    port TEXT,
    UNIQUE(nickname)
);

CREATE INDEX clients_index_nickname ON clients (nickname);

COMMIT;
