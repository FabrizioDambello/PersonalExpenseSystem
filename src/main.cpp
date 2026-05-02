#include <iostream>
#include <libpq-fe.h>
#include <string>
using namespace std;

void creaTabelle(PGconn* conn) {
    const char* Categorie =
        "CREATE TABLE IF NOT EXISTS Categorie ("
        "nome TEXT PRIMARY KEY);";

    const char* Spese =
        "CREATE TABLE IF NOT EXISTS Spese ("
        "id INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY, "
        "data DATE NOT NULL, "
        "importo NUMERIC(10,2) NOT NULL CHECK(importo > 0), "
        "nome_categoria TEXT NOT NULL, "
        "descrizione TEXT, "
        "FOREIGN KEY (nome_categoria) REFERENCES Categorie(nome));";

    const char* Budget =
        "CREATE TABLE IF NOT EXISTS Budget ("
        "mese TEXT NOT NULL, "
        "nome_categoria TEXT NOT NULL, "
        "importo NUMERIC(10,2) NOT NULL CHECK(importo > 0), "
        "PRIMARY KEY (mese, nome_categoria), "
        "FOREIGN KEY (nome_categoria) REFERENCES Categorie(nome));";

    PQexec(conn, Categorie);
    PQexec(conn, Spese);
    PQexec(conn, Budget);
}

void aggiungicategoria(PGconn* conn) {
    string nome;
    cout << "Nome categoria: ";
    cin >> nome;
    if (nome.empty()) {
        cout << "Errore: nome vuoto.\n";
        return;
    }

    string verificanome =
        "SELECT EXISTS (SELECT 1 FROM Categorie WHERE nome = '" + nome + "');";

    PGresult* res = PQexec(conn, verificanome.c_str());

    string verificaselect = PQgetvalue(res, 0, 0);

    if (verificaselect == "t") {
        cout << "La categoria esiste gia'.\n";
        PQclear(res);
        return;
    }

    PQclear(res);

    string Inserimento =
        "INSERT INTO Categorie(nome) VALUES('" + nome + "');";

    PQexec(conn, Inserimento.c_str());

    cout << "Categoria inserita correttamente.\n";

}
void aggiungispesa(PGconn* conn) {
    string data, categoria, descrizione;
    float importo;

    cout << "Data (YYYY-MM-DD): ";
    cin >> data;

    cout << "Categoria: ";
    cin >> categoria;

    cout << "Importo: ";
    cin >> importo;

    cin.ignore();
    cout << "Descrizione: ";
    getline(cin, descrizione);
    if (importo <= 0) {
        cout << "Errore: l'importo deve essere maggiore di zero.\n";
        return;
    }

    const char* checkValues[1];
    checkValues[0] = categoria.c_str();

    PGresult* res = PQexecParams(
        conn,
        "SELECT EXISTS (SELECT 1 FROM Categorie WHERE nome = $1);",
        1,
        NULL,
        checkValues,
        NULL,
        NULL,
        0
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        cout << "Errore: impossibile verificare la categoria.\n";
        cout << PQerrorMessage(conn);
        PQclear(res);
        return;
    }

    if (PQntuples(res) == 0 || PQgetvalue(res, 0, 0)[0] != 't') {
        cout << "Errore: la categoria non esiste.\n";
        PQclear(res);
        return;
    }

    PQclear(res);

    string importo_SPESA = to_string(importo);

    const char* insertValues[4];
    insertValues[0] = data.c_str();
    insertValues[1] = importo_SPESA.c_str();
    insertValues[2] = categoria.c_str();
    insertValues[3] = descrizione.c_str();

    res = PQexecParams(
        conn,
        "INSERT INTO Spese(data, importo, nome_categoria, descrizione) VALUES($1, $2, $3, $4);",
        4,
        NULL,
        insertValues,
        NULL,
        NULL,
        0
    );

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        cout << "Errore nell'inserimento: " << PQerrorMessage(conn) << endl;
        PQclear(res);
        return;
    }

    PQclear(res);

    cout << "Spesa inserita correttamente.\n";
}

void impostabudget(PGconn* conn) {
    string mese, categoria;
    float importo;

    cout << "Mese (YYYY-MM): ";
    cin >> mese;

    cout << "Categoria: ";
    cin >> categoria;

    cout << "Importo budget: ";
    cin >> importo;
    if(importo <= 0) {
        cout << "Errore: importo non valido\n";
        return;
    }

    if (categoria.empty()) {
        cout << "Errore: categoria vuota.\n";
        return;
    }

    const char* checkValues[1];
    checkValues[0] = categoria.c_str();

    PGresult* res = PQexecParams(
        conn,
        "SELECT EXISTS (SELECT 1 FROM Categorie WHERE nome = $1);",
        1,
        NULL,
        checkValues,
        NULL,
        NULL,
        0
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        cout << "Errore: impossibile verificare la categoria.\n";
        cout << PQerrorMessage(conn);
        PQclear(res);
        return;
    }

    if (PQntuples(res) == 0 || PQgetvalue(res, 0, 0)[0] != 't') {
        cout << "Errore: la categoria non esiste.\n";
        PQclear(res);
        return;
    }

    PQclear(res);

    string imp = to_string(importo);

    const char* values[3] = {
        mese.c_str(),
        categoria.c_str(),
        imp.c_str()
    };

    res = PQexecParams(conn,
        "INSERT INTO Budget(mese, nome_categoria, importo) "
        "VALUES($1,$2,$3) "
        "ON CONFLICT (mese, nome_categoria) DO NOTHING;",
        3, NULL, values, NULL, NULL, 0);

    if (PQcmdTuples(res)[0] == '0') {
        cout << "Budget gia' esistente. Non modificato.\n";
    } else {
        cout << "Budget salvato correttamente.\n";
    }

    PQclear(res);
}
void speseCategorie(PGconn* conn) {
    PGresult* res = PQexec(conn,
        "SELECT nome_categoria, SUM(importo) "
        "FROM Spese GROUP BY nome_categoria;");

    for(int i=0; i<PQntuples(res); i++) {
        cout << PQgetvalue(res,i,0) << " - "
             << PQgetvalue(res,i,1) << endl;
    }

    PQclear(res);
}
void budgetvsspese(PGconn* conn) {
    PGresult* res = PQexec(conn,
        "SELECT b.mese, b.nome_categoria, b.importo, "
        "COALESCE(SUM(s.importo),0) "
        "FROM Budget b LEFT JOIN Spese s "
        "ON b.nome_categoria = s.nome_categoria "
        "AND to_char(s.data,'YYYY-MM') = b.mese "
        "GROUP BY b.mese, b.nome_categoria, b.importo;");

    for(int i=0; i<PQntuples(res); i++) {
        double budget = atof(PQgetvalue(res,i,2));
        double speso = atof(PQgetvalue(res,i,3));

        cout << "Mese: " << PQgetvalue(res,i,0) << endl;
        cout << "Categoria: " << PQgetvalue(res,i,1) << endl;
        cout << "Budget: " << budget << endl;
        cout << "Speso: " << speso << endl;

        if(speso > budget)
            cout << "SUPERATO\n\n";
        else
            cout << "OK\n\n";
    }

    PQclear(res);
}
void Speseperdata (PGconn* conn) {
    PGresult* res = PQexec(conn,
        "SELECT *"
        "FROM Spese ORDER BY data;");

    for(int i=0; i<PQntuples(res); i++) {
        cout << PQgetvalue(res,i,0) << " "
             << PQgetvalue(res,i,1) << " "
             << PQgetvalue(res,i,2) << " "
             << PQgetvalue(res,i,3) << endl;
    }

    PQclear(res);
}
void menureport(PGconn* conn) {
    int sceltareport;

    do {
        cout << "\n--- MENU REPORT ---";
        cout << "\n1. Totale spese per categoria";
        cout << "\n2. Spese mensili vs budget";
        cout << "\n3. Elenco completo delle spese ordinate per data";
        cout << "\n4. Menu Principale\n";
        cin >> sceltareport;

        switch (sceltareport) {
            case 1: speseCategorie(conn); break;
            case 2: budgetvsspese(conn); break;
            case 3: Speseperdata(conn); break;
            case 4: return;
            default: cout << "Scelta non valida"; break;
        }

    } while (sceltareport != 0);
}

void menu(PGconn* conn) {
    int scelta;

    do {
        cout << "\n--- MENU ---";
        cout << "\n1. Aggiungi categoria";
        cout << "\n2. Aggiungi spesa";
        cout << "\n3. Imposta budget";
        cout << "\n4. Report mensile";
        cout << "\n0. Esci\n";
        cin >> scelta;

        switch (scelta) {
            case 1: aggiungicategoria(conn); break;
            case 2: aggiungispesa(conn); break;
            case 3: impostabudget(conn); break;
            case 4: menureport(conn); break;
            case 0: cout << "Uscita"; break;
            default: cout << "Scelta non valida"; break;
        }

    } while (scelta != 0);
}


int main() {
    
    const char* conninfo = "host=localhost port=5432 dbname=postgres user=postgres password=Fabrizio";
    PGconn* conn = PQconnectdb(conninfo);
    
    if (PQstatus(conn) != CONNECTION_OK) {
        cout << "Errore di connessione: " << PQerrorMessage(conn) << endl;
        PQfinish(conn);
        return 1;
    }
    cout << "Connessione al database riuscita!" << endl;
    creaTabelle(conn);
    menu(conn);
    PQfinish(conn);
    return 0;
}