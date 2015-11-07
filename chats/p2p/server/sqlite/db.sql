.open /var/chat-p2p-mu/clients.db

BEGIN TRANSACTION;

CREATE TABLE clients (
    id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
    nickname CHAR(24) NOT NULL,
    keyword CHAR(40) NOT NULL,
    host CHAR(64) NOT NULL,
    port CHAR(5) NOT NULL,
    UNIQUE(nickname)
);

CREATE INDEX clients_index_nickname ON clients (nickname);

COMMIT;
