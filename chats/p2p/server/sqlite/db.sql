.open /var/chat-p2p-mu/clients.db

BEGIN TRANSACTION;

CREATE TABLE clients (
    id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
    nickname CHAR(24),
    keyword CHAR(40),
    host TEXT,
    port INTEGER,
    UNIQUE(nickname)
);

COMMIT;
