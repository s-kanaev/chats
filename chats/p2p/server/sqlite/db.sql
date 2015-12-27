.open /var/chat-p2p-mu/clients.db

BEGIN TRANSACTION;

CREATE TABLE clients (
    id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
    nickname CHAR(16) NOT NULL,
    host TEXT NOT NULL,
    port TEXT NOT NULL,
    UNIQUE(nickname)
);

CREATE INDEX clients_index_nickname ON clients (nickname);

COMMIT;
