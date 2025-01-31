#include <iostream>
#include <pqxx/pqxx>
#include <cstdlib>

using namespace std;
using namespace pqxx;

// Funciones de utilidad
void limpiarPantalla() {
    system("clear");
}

void limpiarEntrada(){
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void esperarEntrada() {
    cout << "\nPresione Enter para continuar...";
    cin.get();
}

// Función para ejecutar consultas SQL 
bool ejecutarSQL(connection &c, const string &query) {
    try {
        work W(c);
        W.exec(query);
        W.commit();
        return true;
    } catch (const exception &e) {
        cerr << "\033[1;31mError en la consulta SQL: " << e.what() << "\033[0m" << endl;
        return false;
    }
}

connection conectar() {
    const string CONFIGURACION_DB = 
        "dbname=grandslam "
        "user=postgres "
        "password=1234admin "
        "hostaddr=127.0.0.1 "
        "port=5432";

    try {
        connection c(CONFIGURACION_DB);
        if (!c.is_open()) {
            throw runtime_error("No se pudo establecer la conexión con la base de datos");
        }
        cout << "Conexión establecida con la base de datos: " << c.dbname() << endl;
        return c;
    } catch (const exception& e) {
        throw runtime_error("Error al conectar: " + string(e.what()));
    }
    esperarEntrada();
}

void comprobarTablas(connection &c) {
    try {
        // Países
        string query_countries = "CREATE TABLE IF NOT EXISTS Countries ("
                               "country_id SERIAL PRIMARY KEY, "
                               "name VARCHAR(100) NOT NULL, "
                               "UNIQUE(name)"
                               ");";
        ejecutarSQL(c, query_countries);

        // Sedes/Venues
        string query_venues = "CREATE TABLE IF NOT EXISTS Venues ("
                            "venue_id SERIAL PRIMARY KEY, "
                            "name VARCHAR(100) NOT NULL, "
                            "country_id INTEGER REFERENCES Countries(country_id), "
                            "city VARCHAR(100) NOT NULL, "
                            "is_active BOOLEAN DEFAULT true, "
                            "UNIQUE(name, country_id)"
                            ");";
        ejecutarSQL(c, query_venues);

        // Categorías
        string query_categories = "CREATE TABLE IF NOT EXISTS Categories ("
                                "category_id SERIAL PRIMARY KEY, "
                                "name VARCHAR(50) NOT NULL, "
                                "UNIQUE(name)"
                                ");";
        ejecutarSQL(c, query_categories);

        // Configuración de Categorías
        string query_category_configs = "CREATE TABLE IF NOT EXISTS CategoryConfigs ("
                                      "category_id INTEGER REFERENCES Categories(category_id), "
                                      "allowed_gender CHAR(1) NOT NULL CHECK (allowed_gender IN ('M', 'F', 'B')), "
                                      "players_per_team INTEGER NOT NULL CHECK (players_per_team IN (1, 2)), "
                                      "PRIMARY KEY (category_id)"
                                      ");";
        ejecutarSQL(c, query_category_configs);

        // Torneos
        string query_tournaments = "CREATE TABLE IF NOT EXISTS Tournaments ("
                                 "tournament_id SERIAL PRIMARY KEY, "
                                 "name VARCHAR(100) NOT NULL, "
                                 "country_id INTEGER REFERENCES Countries(country_id), "
                                 "UNIQUE(name)"
                                 ");";
        ejecutarSQL(c, query_tournaments);

        // Jugadores
        string query_players = "CREATE TABLE IF NOT EXISTS Players ("
                             "player_id SERIAL PRIMARY KEY, "
                             "name VARCHAR(100) NOT NULL, "
                             "gender CHAR(1) NOT NULL CHECK (gender IN ('M', 'F')), "
                             "birth_date DATE, "
                             "UNIQUE(name)"
                             ");";
        ejecutarSQL(c, query_players);

        // Nacionalidades de jugadores
        string query_player_nationalities = "CREATE TABLE IF NOT EXISTS PlayerNationalities ("
                                          "player_id INTEGER REFERENCES Players(player_id), "
                                          "country_id INTEGER REFERENCES Countries(country_id), "
                                          "PRIMARY KEY (player_id, country_id)"
                                          ");";
        ejecutarSQL(c, query_player_nationalities);

        // Árbitros
        string query_referees = "CREATE TABLE IF NOT EXISTS Referees ("
                              "referee_id SERIAL PRIMARY KEY, "
                              "name VARCHAR(100) NOT NULL, "
                              "country_id INTEGER REFERENCES Countries(country_id), "
                              "active BOOLEAN DEFAULT true, "
                              "UNIQUE(name)"
                              ");";
        ejecutarSQL(c, query_referees);

        // Entrenadores
        string query_coaches = "CREATE TABLE IF NOT EXISTS Coaches ("
                             "coach_id SERIAL PRIMARY KEY, "
                             "name VARCHAR(100) NOT NULL, "
                             "country_id INTEGER REFERENCES Countries(country_id), "
                             "active BOOLEAN DEFAULT true, "
                             "UNIQUE(name)"
                             ");";
        ejecutarSQL(c, query_coaches);

        // Historial de entrenadores
        string query_coaching_history = "CREATE TABLE IF NOT EXISTS CoachingHistory ("
                                      "player_id INTEGER REFERENCES Players(player_id), "
                                      "coach_id INTEGER REFERENCES Coaches(coach_id), "
                                      "start_date DATE NOT NULL, "
                                      "end_date DATE, "
                                      "CHECK (end_date IS NULL OR end_date >= start_date), "
                                      "PRIMARY KEY (player_id, coach_id, start_date)"
                                      ");";
        ejecutarSQL(c, query_coaching_history);

        // Ediciones de torneo
        string query_tournament_editions = "CREATE TABLE IF NOT EXISTS TournamentEditions ("
                                         "edition_id SERIAL PRIMARY KEY, "
                                         "tournament_id INTEGER REFERENCES Tournaments(tournament_id), "
                                         "year INTEGER NOT NULL, "
                                         "venue_id INTEGER REFERENCES Venues(venue_id), "
                                         "start_date DATE NOT NULL, "
                                         "end_date DATE NOT NULL, "
                                         "CHECK (end_date >= start_date), "
                                         "UNIQUE(tournament_id, year)"
                                         ");";
        ejecutarSQL(c, query_tournament_editions);

        // Configuración de premios por ronda
        string query_prize_money = "CREATE TABLE IF NOT EXISTS PrizeMoney ("
                                 "edition_id INTEGER REFERENCES TournamentEditions(edition_id), "
                                 "round VARCHAR(50) NOT NULL, "
                                 "winner_prize DECIMAL(10,2) NOT NULL CHECK (winner_prize > 0), "
                                 "loser_prize DECIMAL(10,2) NOT NULL CHECK (loser_prize > 0), "
                                 "CHECK (winner_prize >= loser_prize), "
                                 "PRIMARY KEY (edition_id, round)"
                                 ");";
        ejecutarSQL(c, query_prize_money);

        // Partidos
        string query_matches = "CREATE TABLE IF NOT EXISTS Matches ("
                             "match_id SERIAL PRIMARY KEY, "
                             "edition_id INTEGER REFERENCES TournamentEditions(edition_id), "
                             "category_id INTEGER REFERENCES Categories(category_id), "
                             "round VARCHAR(50) NOT NULL, "
                             "date DATE NOT NULL, "
                             "referee_id INTEGER REFERENCES Referees(referee_id), "
                             "winner_prize DECIMAL(10,2) NOT NULL CHECK (winner_prize > 0), "
                             "loser_prize DECIMAL(10,2) NOT NULL CHECK (loser_prize > 0), "
                             "CHECK (winner_prize >= loser_prize)"
                             ");";
        ejecutarSQL(c, query_matches);

        // Participantes en partidos
        string query_match_participants = "CREATE TABLE IF NOT EXISTS MatchParticipants ("
                                        "match_id INTEGER REFERENCES Matches(match_id), "
                                        "player_id INTEGER REFERENCES Players(player_id), "
                                        "team_number INTEGER NOT NULL CHECK (team_number IN (1, 2)), "
                                        "is_winner BOOLEAN NOT NULL, "
                                        "score VARCHAR(50) NOT NULL, "
                                        "PRIMARY KEY (match_id, player_id)"
                                        ");";
        ejecutarSQL(c, query_match_participants);

        // Insertar datos por defecto

        // Categorías por defecto
        string query_default_categories = "INSERT INTO Categories (name) "
                                        "VALUES ('Individual masculino'), ('Individual femenino'), "
                                        "('Dobles masculino'), ('Dobles femenino'), ('Dobles mixtos') "
                                        "ON CONFLICT (name) DO NOTHING;";
        ejecutarSQL(c, query_default_categories);

        // Configuración de categorías por defecto
        string query_default_category_configs = 
            "INSERT INTO CategoryConfigs (category_id, allowed_gender, players_per_team) "
            "SELECT category_id, "
            "CASE "
                "WHEN name LIKE '%masculino%' THEN 'M' "
                "WHEN name LIKE '%femenino%' THEN 'F' "
                "WHEN name LIKE '%mixtos%' THEN 'B' "
            "END, "
            "CASE "
                "WHEN name LIKE 'Dobles%' THEN 2 "
                "ELSE 1 "
            "END "
            "FROM Categories "
            "ON CONFLICT (category_id) DO NOTHING;";
        ejecutarSQL(c, query_default_category_configs);

        // Países principales por defecto
        string query_default_countries = "INSERT INTO Countries (name) "
                                       "VALUES ('Gran Bretaña'), ('Estados Unidos'), "
                                       "('Francia'), ('Australia') "
                                       "ON CONFLICT (name) DO NOTHING;";
        ejecutarSQL(c, query_default_countries);

        // Sedes principales por defecto
        string query_default_venues = 
            "DO $$ "
            "DECLARE "
                "gb_id INTEGER; "
                "us_id INTEGER; "
                "fr_id INTEGER; "
                "au_id INTEGER; "
            "BEGIN "
                "SELECT country_id INTO gb_id FROM Countries WHERE name = 'Gran Bretaña'; "
                "SELECT country_id INTO us_id FROM Countries WHERE name = 'Estados Unidos'; "
                "SELECT country_id INTO fr_id FROM Countries WHERE name = 'Francia'; "
                "SELECT country_id INTO au_id FROM Countries WHERE name = 'Australia'; "

                "INSERT INTO Venues (name, country_id, city) VALUES "
                "('All England Club', gb_id, 'Londres'), "
                "('Flushing Meadows', us_id, 'Nueva York'), "
                "('Forest Hills', us_id, 'Nueva York'), "
                "('Roland Garros', fr_id, 'París'), "
                "('Melbourne Park', au_id, 'Melbourne') "
                "ON CONFLICT (name, country_id) DO NOTHING; "
            "END $$;";
        ejecutarSQL(c, query_default_venues);

        // Torneos principales por defecto
        string query_default_tournaments = 
            "DO $$ "
            "DECLARE "
                "gb_id INTEGER; "
                "us_id INTEGER; "
                "fr_id INTEGER; "
                "au_id INTEGER; "
            "BEGIN "
                "SELECT country_id INTO gb_id FROM Countries WHERE name = 'Gran Bretaña'; "
                "SELECT country_id INTO us_id FROM Countries WHERE name = 'Estados Unidos'; "
                "SELECT country_id INTO fr_id FROM Countries WHERE name = 'Francia'; "
                "SELECT country_id INTO au_id FROM Countries WHERE name = 'Australia'; "
                
                "INSERT INTO Tournaments (name, country_id) VALUES "
                "('Wimbledon', gb_id), "
                "('US Open', us_id), "
                "('Roland Garros', fr_id), "
                "('Australian Open', au_id) "
                "ON CONFLICT (name) DO NOTHING; "
            "END $$;";
        ejecutarSQL(c, query_default_tournaments);

        cout << "Base de datos inicializada correctamente" << endl;
        system("sleep 1");
        cout << "Iniciando sistema de gestión..." << endl;
        system("sleep 2");
    } catch (const exception &e) {
        cerr << "Error durante la inicialización de tablas: " << e.what() << endl;
        esperarEntrada();
        throw;
    }
}

// Funciones auxiliares
// Función auxiliar para verificar si un jugador ya tiene un partido en una fecha
bool tienePartidoEnFecha(work &W, int player_id, const string &fecha) {
    result R = W.exec(
        "SELECT COUNT(*) FROM MatchParticipants mp "
        "JOIN Matches m ON mp.match_id = m.match_id "
        "WHERE mp.player_id = " + to_string(player_id) + " "
        "AND m.date = '" + fecha + "';"
    );
    return R[0][0].as<int>() > 0;
}

// Función auxiliar para obtener el género de un jugador
char obtenerGeneroJugador(work &W, int player_id) {
    result R = W.exec(
        "SELECT gender FROM Players WHERE player_id = " + to_string(player_id) + ";"
    );
    return R[0][0].as<string>()[0];
}

// Función auxiliar para validar la composición del equipo según la categoría
bool validarEquipo(work &W, const vector<int> &jugadores, const string &categoria_nombre) {
    if (categoria_nombre == "Individual masculino") {
        return jugadores.size() == 1 && obtenerGeneroJugador(W, jugadores[0]) == 'M';
    } else if (categoria_nombre == "Individual femenino") {
        return jugadores.size() == 1 && obtenerGeneroJugador(W, jugadores[0]) == 'F';
    } else if (categoria_nombre == "Dobles masculino") {
        return jugadores.size() == 2 && 
               obtenerGeneroJugador(W, jugadores[0]) == 'M' && 
               obtenerGeneroJugador(W, jugadores[1]) == 'M';
    } else if (categoria_nombre == "Dobles femenino") {
        return jugadores.size() == 2 && 
               obtenerGeneroJugador(W, jugadores[0]) == 'F' && 
               obtenerGeneroJugador(W, jugadores[1]) == 'F';
    } else if (categoria_nombre == "Dobles mixtos") {
        char genero1 = obtenerGeneroJugador(W, jugadores[0]);
        char genero2 = obtenerGeneroJugador(W, jugadores[1]);
        return jugadores.size() == 2 && genero1 != genero2;
    }
    return false;
}

// Función auxiliar para obtener los premios según la ronda y edición
bool obtenerPremios(work &W, int edition_id, const string &ronda, double &winner_prize, double &loser_prize) {
    result R = W.exec(
        "SELECT winner_prize, loser_prize FROM PrizeMoney "
        "WHERE edition_id = " + to_string(edition_id) + " "
        "AND round = '" + ronda + "';"
    );
    
    if (R.empty()) {
        // Si no hay premios configurados, usar valores por defecto según la ronda
        if (ronda == "Final") {
            winner_prize = 100000; loser_prize = 50000;
        } else if (ronda == "Semifinal") {
            winner_prize = 50000; loser_prize = 25000;
        } else if (ronda == "Cuartos") {
            winner_prize = 25000; loser_prize = 12500;
        } else if (ronda == "Octavos") {
            winner_prize = 12500; loser_prize = 6250;
        } else {
            winner_prize = 6250; loser_prize = 3125;
        }
        
        // Insertar los premios en la tabla
        W.exec(
            "INSERT INTO PrizeMoney (edition_id, round, winner_prize, loser_prize) VALUES (" +
            to_string(edition_id) + ", '" + ronda + "', " + 
            to_string(winner_prize) + ", " + to_string(loser_prize) + ");"
        );
        return true;
    }
    
    winner_prize = R[0]["winner_prize"].as<double>();
    loser_prize = R[0]["loser_prize"].as<double>();
    return true;
}

// Funciones de visualización
void consultarTorneos(connection &c) {
    limpiarPantalla();
    try {
        string query = 
            "SELECT te.year, t.name as tournament, v.name as venue, "
            "c.name as country, te.start_date, te.end_date, "
            "COUNT(DISTINCT m.match_id) as total_matches "
            "FROM TournamentEditions te "
            "JOIN Tournaments t ON te.tournament_id = t.tournament_id "
            "JOIN Venues v ON te.venue_id = v.venue_id "
            "JOIN Countries c ON t.country_id = c.country_id "
            "LEFT JOIN Matches m ON te.edition_id = m.edition_id "
            "GROUP BY te.year, t.name, v.name, c.name, te.start_date, te.end_date "
            "ORDER BY te.year DESC, t.name;";

        nontransaction N(c);
        result R(N.exec(query));

        if (R.empty()) {
            cout << "No hay ediciones de torneos registradas." << endl;
            esperarEntrada();
            return;
        }

        cout << "\033[1;33m╔═══════════════════════════════════════╗\033[0m" << endl;
        cout << "\033[1;33m║      HISTÓRICO DE GRAND SLAMS         ║\033[0m" << endl;
        cout << "\033[1;33m╚═══════════════════════════════════════╝\033[0m" << endl;

        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "\n\033[1;36m▶ " << row["year"].as<string>() << " - " 
                 << row["tournament"].as<string>() << "\033[0m" << endl;
            cout << "  📍 Sede: " << row["venue"].as<string>() << ", " 
                 << row["country"].as<string>() << endl;
            cout << "  📅 Fecha: " << row["start_date"].as<string>() << " al " 
                 << row["end_date"].as<string>() << endl;
            cout << "  🎾 Partidos jugados: " << row["total_matches"].as<int>() << endl;
            cout << "  " << string(40, '-') << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al consultar torneos: " << e.what() << endl;
        esperarEntrada();
        throw;
    }
    esperarEntrada();
}

void consultarJugadores(connection &c) {
    limpiarPantalla();
    try {
        string query = 
            "SELECT p.player_id, p.name, p.birth_date, "
            "COALESCE(string_agg(DISTINCT c.name, ', '), 'Sin nacionalidad') as nationalities, "
            "COUNT(DISTINCT mp.match_id) as matches_played, "
            "COUNT(DISTINCT CASE WHEN mp.is_winner THEN mp.match_id END) as matches_won "
            "FROM Players p "
            "LEFT JOIN PlayerNationalities pn ON p.player_id = pn.player_id "
            "LEFT JOIN Countries c ON pn.country_id = c.country_id "
            "LEFT JOIN MatchParticipants mp ON p.player_id = mp.player_id "
            "GROUP BY p.player_id, p.name, p.birth_date "
            "ORDER BY p.name;";

        nontransaction N(c);
        result R(N.exec(query));

        if (R.empty()) {
            cout << "No hay jugadores registrados." << endl;
            esperarEntrada();
            return;
        }

        cout << "\033[1;33m╔═══════════════════════════════════════╗\033[0m" << endl;
        cout << "\033[1;33m║         DIRECTORIO DE JUGADORES       ║\033[0m" << endl;
        cout << "\033[1;33m╚═══════════════════════════════════════╝\033[0m" << endl;

        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "\n\033[1;36m▶ " << row["name"].as<string>() << "\033[0m" << endl;
            cout << "  🌍 Nacionalidad(es): " << row["nationalities"].as<string>() << endl;
            if (!row["birth_date"].is_null()) {
                cout << "  📅 Fecha de nacimiento: " << row["birth_date"].as<string>() << endl;
            }
            cout << "  🎾 Partidos jugados: " << row["matches_played"].as<int>() << endl;
            cout << "  🏆 Partidos ganados: " << row["matches_won"].as<int>() << endl;
            cout << "  " << string(40, '-') << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al consultar jugadores: " << e.what() << endl;
        esperarEntrada();
        throw;
    }
    esperarEntrada();
}

void consultarArbitros(connection &c) {
    limpiarPantalla();
    try {
        string query = 
            "SELECT r.referee_id, r.name, c.name as country, "
            "COUNT(DISTINCT m.match_id) as matches_officiated "
            "FROM Referees r "
            "LEFT JOIN Countries c ON r.country_id = c.country_id "
            "LEFT JOIN Matches m ON r.referee_id = m.referee_id "
            "GROUP BY r.referee_id, r.name, c.name "
            "ORDER BY r.name;";

        nontransaction N(c);
        result R(N.exec(query));

        if (R.empty()) {
            cout << "No hay árbitros registrados." << endl;
            esperarEntrada();
            return;
        }

        cout << "\033[1;33m╔═══════════════════════════════════════╗\033[0m" << endl;
        cout << "\033[1;33m║         DIRECTORIO DE ÁRBITROS        ║\033[0m" << endl;
        cout << "\033[1;33m╚═══════════════════════════════════════╝\033[0m" << endl;

        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "\n\033[1;36m▶ " << row["name"].as<string>() << "\033[0m" << endl;
            cout << "  🌍 País: " << row["country"].as<string>() << endl;
            cout << "  🎾 Partidos arbitrados: " << row["matches_officiated"].as<int>() << endl;
            cout << "  " << string(40, '-') << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al consultar árbitros: " << e.what() << endl;
        esperarEntrada();
        throw;
    }
    esperarEntrada();
}

void consultarPremios(connection &c) {
    limpiarPantalla();
    try {
        string query = 
            "SELECT p.name, "
            "SUM(CASE WHEN mp.is_winner THEN m.winner_prize ELSE m.loser_prize END) as total_prize, "
            "COUNT(DISTINCT CASE WHEN mp.is_winner THEN m.match_id END) as tournaments_won "
            "FROM Players p "
            "JOIN MatchParticipants mp ON p.player_id = mp.player_id "
            "JOIN Matches m ON mp.match_id = m.match_id "
            "GROUP BY p.player_id, p.name "
            "HAVING SUM(CASE WHEN mp.is_winner THEN m.winner_prize ELSE m.loser_prize END) > 0 "
            "ORDER BY total_prize DESC;";

        nontransaction N(c);
        result R(N.exec(query));

        if (R.empty()) {
            cout << "No hay registros de premios." << endl;
            esperarEntrada();
            return;
        }

        cout << "\033[1;33m╔═══════════════════════════════════════╗\033[0m" << endl;
        cout << "\033[1;33m║         HISTÓRICO DE PREMIOS          ║\033[0m" << endl;
        cout << "\033[1;33m╚═══════════════════════════════════════╝\033[0m" << endl;

        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "\n\033[1;36m▶ " << row["name"].as<string>() << "\033[0m" << endl;
            cout << "  💰 Premio total: $" << fixed << setprecision(2) 
                 << row["total_prize"].as<double>() << endl;
            cout << "  🏆 Torneos ganados: " << row["tournaments_won"].as<int>() << endl;
            cout << "  " << string(40, '-') << endl;        }
    } catch (const exception &e) {
        cerr << "Error al consultar premios: " << e.what() << endl;
        esperarEntrada();
        throw;
    }
    esperarEntrada();
}

void consultarPartidosPorTorneo(connection &c) {
    limpiarPantalla();
    try {
        work W(c);
        cout << "\033[1;33m╔═══════════════════════════════════════╗\033[0m" << endl;
        cout << "\033[1;33m║       CONSULTA DE PARTIDOS            ║\033[0m" << endl;
        cout << "\033[1;33m╚═══════════════════════════════════════╝\033[0m" << endl;

        // Obtener lista de torneos con años disponibles
        result R_tournaments = W.exec(
            "WITH EdicionesConPartidos AS ("
            "    SELECT DISTINCT te.tournament_id, te.year "
            "    FROM TournamentEditions te "
            "    JOIN Matches m ON te.edition_id = m.edition_id "
            ")"
            "SELECT t.tournament_id, t.name, e.year "
            "FROM Tournaments t "
            "JOIN EdicionesConPartidos e ON t.tournament_id = e.tournament_id "
            "ORDER BY t.name, e.year DESC;"
        );

        if (R_tournaments.empty()) {
            cout << "\n\033[1;31mNo hay partidos registrados en ningún torneo.\033[0m" << endl;
            esperarEntrada();
            return;
        }

        cout << "\nTorneos disponibles:\n" << string(50, '-') << endl;
        string current_tournament = "";
        for (result::const_iterator row = R_tournaments.begin(); row != R_tournaments.end(); ++row) {
            string tournament_name = row["name"].as<string>();
            if (tournament_name != current_tournament) {
                current_tournament = tournament_name;
                cout << "\n" << tournament_name << ":" << endl;
            }
            cout << "  - " << row["year"].as<string>() 
                 << " (ID: " << row["tournament_id"].as<int>() << ")" << endl;
        }
        cout << string(50, '-') << endl;

        int tournament_id;
        cout << "\nSeleccione ID del torneo: ";
        cin >> tournament_id;
        limpiarEntrada();

        int year;
        cout << "Ingrese el año: ";
        cin >> year;
        limpiarEntrada();

        // Verificar que exista la combinación torneo-año
        result R_check = W.exec(
            "SELECT COUNT(*) FROM TournamentEditions te "
            "WHERE te.tournament_id = " + to_string(tournament_id) + " "
            "AND te.year = " + to_string(year) + ";"
        );

        if (R_check[0][0].as<int>() == 0) {
            cout << "\n\033[1;31mNo se encontró el torneo para el año especificado.\033[0m" << endl;
            esperarEntrada();
            return;
        }

        string query = 
            "WITH MatchDetails AS ( "
            "    SELECT m.match_id, c.name as categoria, m.round, m.date, "
            "           string_agg(DISTINCT CASE "
            "               WHEN mp.team_number = 1 THEN p.name "
            "               END, ' / ') as equipo1, "
            "           string_agg(DISTINCT CASE "
            "               WHEN mp.team_number = 2 THEN p.name "
            "               END, ' / ') as equipo2, "
            "           MAX(CASE WHEN mp.team_number = 1 THEN mp.score END) as score, "
            "           r.name as arbitro, "
            "           MAX(CASE WHEN mp.team_number = mp.is_winner::int THEN m.winner_prize "
            "               ELSE m.loser_prize END) as premio, "
            "           bool_or(CASE WHEN mp.team_number = 1 THEN mp.is_winner END) as equipo1_ganador "
            "    FROM Matches m "
            "    JOIN TournamentEditions te ON m.edition_id = te.edition_id "
            "    JOIN Tournaments t ON te.tournament_id = t.tournament_id "
            "    JOIN Categories c ON m.category_id = c.category_id "
            "    JOIN MatchParticipants mp ON m.match_id = mp.match_id "
            "    JOIN Players p ON mp.player_id = p.player_id "
            "    JOIN Referees r ON m.referee_id = r.referee_id "
            "    WHERE t.tournament_id = $1 AND te.year = $2 "
            "    GROUP BY m.match_id, c.name, m.round, m.date, r.name "
            ") "
            "SELECT *, "
            "CASE WHEN equipo1_ganador THEN equipo1 ELSE equipo2 END as ganador, "
            "CASE WHEN equipo1_ganador THEN equipo2 ELSE equipo1 END as perdedor "
            "FROM MatchDetails "
            "ORDER BY date, categoria;";

        result R = W.exec_params(query, tournament_id, year);

        if (R.empty()) {
            cout << "\n\033[1;31mNo se encontraron partidos registrados para este torneo y año.\033[0m" << endl;
            esperarEntrada();
            return;
        }

        // Obtener nombre del torneo
        string tournament_name = W.exec(
            "SELECT name FROM Tournaments WHERE tournament_id = " + 
            to_string(tournament_id) + ";"
        )[0][0].as<string>();

        cout << "\n\033[1;36mPartidos de " << tournament_name << " " << year << "\033[0m\n" << endl;

        string current_category = "";
        string current_round = "";

        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            string categoria = row["categoria"].as<string>();
            string ronda = row["round"].as<string>();

            // Imprimir encabezados de categoría y ronda cuando cambian
            if (categoria != current_category) {
                cout << "\n\033[1;33m=== " << categoria << " ===\033[0m" << endl;
                current_category = categoria;
                current_round = "";
            }
            
            if (ronda != current_round) {
                cout << "\n\033[1;32m>> " << ronda << "\033[0m" << endl;
                current_round = ronda;
            }

            cout << "\n📅 " << row["date"].as<string>() << endl;
            cout << "👥 " << row["equipo1"].as<string>() << " vs " << row["equipo2"].as<string>() << endl;
            cout << "🎯 Resultado: " << row["score"].as<string>() << endl;
            cout << "🏆 Ganador: " << row["ganador"].as<string>() << endl;
            cout << "💰 Premio: $" << fixed << setprecision(2) << row["premio"].as<double>() << endl;
            cout << "👨‍⚖️ Árbitro: " << row["arbitro"].as<string>() << endl;
            cout << string(50, '-') << endl;
        }

    } catch (const exception &e) {
        cerr << "\n\033[1;31mError al consultar partidos: " << e.what() << "\033[0m" << endl;
    }
    esperarEntrada();
}

void consultarHistorialEntrenadores(connection &c) {
    limpiarPantalla();
    try {
        work W(c);
        cout << "\033[1;33m╔═══════════════════════════════════════╗\033[0m" << endl;
        cout << "\033[1;33m║      HISTORIAL DE ENTRENADORES        ║\033[0m" << endl;
        cout << "\033[1;33m╚═══════════════════════════════════════╝\033[0m" << endl;

        // Obtener jugadores que tienen o han tenido entrenadores
        result R_players = W.exec(
            "SELECT DISTINCT p.player_id, p.name, "
            "string_agg(DISTINCT c.name, ', ') as nationalities "
            "FROM Players p "
            "LEFT JOIN PlayerNationalities pn ON p.player_id = pn.player_id "
            "LEFT JOIN Countries c ON pn.country_id = c.country_id "
            "LEFT JOIN CoachingHistory ch ON p.player_id = ch.player_id "
            "GROUP BY p.player_id, p.name "
            "HAVING COUNT(ch.coach_id) > 0 "
            "ORDER BY p.name;"
        );

        if (R_players.empty()) {
            cout << "\n\033[1;31mNo hay jugadores con historial de entrenadores registrados.\033[0m" << endl;
            esperarEntrada();
            return;
        }

        cout << "\nJugadores disponibles:\n" << string(70, '-') << endl;
        for (result::const_iterator row = R_players.begin(); row != R_players.end(); ++row) {
            cout << "ID: " << row["player_id"].as<int>() 
                 << " | " << row["name"].as<string>() 
                 << " [" << row["nationalities"].as<string>() << "]" << endl;
        }
        cout << string(70, '-') << endl;

        int player_id;
        cout << "\nSeleccione ID del jugador: ";
        cin >> player_id;
        limpiarEntrada();

        // Query principal usando CTE para organizar la información
        string query = 
            "WITH CoachStats AS ( "
            "    SELECT "
            "        c.coach_id, "
            "        c.name as coach_name, "
            "        co.name as country, "
            "        ch.start_date, "
            "        ch.end_date, "
            "        COUNT(DISTINCT m.match_id) as matches_during_period, "
            "        COUNT(DISTINCT CASE WHEN mp.is_winner THEN m.match_id END) as matches_won, "
            "        SUM(CASE WHEN mp.is_winner THEN m.winner_prize ELSE m.loser_prize END) as total_earnings, "
            "        string_agg(DISTINCT cat.name, ', ' ORDER BY cat.name) as categories "
            "    FROM Coaches c "
            "    JOIN Countries co ON c.country_id = co.country_id "
            "    JOIN CoachingHistory ch ON c.coach_id = ch.coach_id "
            "    LEFT JOIN MatchParticipants mp ON ch.player_id = mp.player_id "
            "    LEFT JOIN Matches m ON mp.match_id = m.match_id "
            "        AND m.date BETWEEN ch.start_date AND COALESCE(ch.end_date, CURRENT_DATE) "
            "    LEFT JOIN Categories cat ON m.category_id = cat.category_id "
            "    WHERE ch.player_id = $1 "
            "    GROUP BY c.coach_id, c.name, co.name, ch.start_date, ch.end_date "
            "    ORDER BY ch.start_date DESC "
            ") "
            "SELECT *, "
            "    CASE "
            "        WHEN end_date IS NULL THEN current_date - start_date "
            "        ELSE end_date - start_date "
            "    END as duration "
            "FROM CoachStats;";

        result R = W.exec_params(query, player_id);

        if (R.empty()) {
            cout << "\n\033[1;31mNo se encontró historial de entrenadores para este jugador.\033[0m" << endl;
            esperarEntrada();
            return;
        }

        string player_name = W.exec(
            "SELECT name FROM Players WHERE player_id = " + to_string(player_id) + ";"
        )[0][0].as<string>();

        cout << "\n\033[1;36mHistorial de entrenadores de " << player_name << "\033[0m\n" << endl;

        // Mostrar información por entrenador
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "\n\033[1;32m▶ " << row["coach_name"].as<string>() << "\033[0m" << endl;
            cout << "  🌍 País: " << row["country"].as<string>() << endl;
            cout << "  📅 Período: " << row["start_date"].as<string>() << " a ";
            if (row["end_date"].is_null()) {
                cout << "Presente";
            } else {
                cout << row["end_date"].as<string>();
            }
            cout << " (" << row["duration"].as<int>() << " días)" << endl;

            if (!row["categories"].is_null()) {
                cout << "  🎾 Categorías: " << row["categories"].as<string>() << endl;
                cout << "  📊 Partidos durante el período: " << row["matches_during_period"].as<int>() << endl;
                cout << "  ✅ Victorias: " << row["matches_won"].as<int>() << endl;
                double win_rate = row["matches_during_period"].as<int>() > 0 ? 
                    (row["matches_won"].as<double>() * 100.0 / row["matches_during_period"].as<int>()) : 0;
                cout << "  📈 Porcentaje victoria: " << fixed << setprecision(1) << win_rate << "%" << endl;
                cout << "  💰 Ganancias del período: $" << fixed << setprecision(2) 
                     << row["total_earnings"].as<double>() << endl;
            }

            // Obtener detalle de partidos durante el período
            result R_matches = W.exec_params(
                "SELECT m.date, cat.name as category, m.round, "
                "mp.score, mp.is_winner, "
                "m.winner_prize, m.loser_prize "
                "FROM Matches m "
                "JOIN Categories cat ON m.category_id = cat.category_id "
                "JOIN MatchParticipants mp ON m.match_id = mp.match_id "
                "WHERE mp.player_id = $1 "
                "AND m.date BETWEEN $2 AND COALESCE($3, CURRENT_DATE) "
                "ORDER BY m.date;",
                player_id, 
                row["start_date"].as<string>(),
                row["end_date"].is_null() ? "CURRENT_DATE" : row["end_date"].as<string>()
            );

            if (!R_matches.empty()) {
                cout << "\n  📋 Detalle de partidos:" << endl;
                for (result::const_iterator match = R_matches.begin(); match != R_matches.end(); ++match) {
                    cout << "    • " << match["date"].as<string>() 
                         << " | " << match["category"].as<string>()
                         << " | " << match["round"].as<string>()
                         << " | " << match["score"].as<string>()
                         << " | " << (match["is_winner"].as<bool>() ? "\033[1;32mVictoria\033[0m" : "\033[1;31mDerrota\033[0m")
                         << " | $" << fixed << setprecision(2)
                         << (match["is_winner"].as<bool>() ? 
                             match["winner_prize"].as<double>() : match["loser_prize"].as<double>())
                         << endl;
                }
            }
            
            cout << string(70, '-') << endl;
        }

    } catch (const exception &e) {
        cerr << "\n\033[1;31mError al consultar historial de entrenadores: " << e.what() << "\033[0m" << endl;
    }
    esperarEntrada();
}

void consultarArbitrosPorTorneo(connection &c) {
    limpiarPantalla();
    try {
        work W(c);
        cout << "\033[1;33m╔═══════════════════════════════════════╗\033[0m" << endl;
        cout << "\033[1;33m║     CONSULTA DE ÁRBITROS POR TORNEO   ║\033[0m" << endl;
        cout << "\033[1;33m╚═══════════════════════════════════════╝\033[0m" << endl;

        // Obtener torneos disponibles con sus años
        result R_tournaments = W.exec(
            "WITH EdicionesConPartidos AS ("
            "    SELECT DISTINCT te.tournament_id, te.year, te.edition_id "
            "    FROM TournamentEditions te "
            "    JOIN Matches m ON te.edition_id = m.edition_id "
            ")"
            "SELECT t.tournament_id, t.name, e.year, e.edition_id "
            "FROM Tournaments t "
            "JOIN EdicionesConPartidos e ON t.tournament_id = e.tournament_id "
            "ORDER BY t.name, e.year DESC;"
        );

        if (R_tournaments.empty()) {
            cout << "\n\033[1;31mNo hay torneos con partidos registrados.\033[0m" << endl;
            esperarEntrada();
            return;
        }

        cout << "\nTorneos disponibles:\n" << string(50, '-') << endl;
        string current_tournament = "";
        for (result::const_iterator row = R_tournaments.begin(); row != R_tournaments.end(); ++row) {
            string tournament_name = row["name"].as<string>();
            if (tournament_name != current_tournament) {
                current_tournament = tournament_name;
                cout << "\n" << tournament_name << ":" << endl;
            }
            cout << "  - " << row["year"].as<string>() 
                << " (ID: " << row["tournament_id"].as<int>() << ")" << endl;
        }

        cout << string(50, '-') << endl;

        int tournament_id;
        cout << "\nSeleccione ID del torneo: ";
        cin >> tournament_id;
        limpiarEntrada();

        int year;
        cout << "Ingrese el año: ";
        cin >> year;
        limpiarEntrada();

        string query = 
            "WITH ArbitroStats AS ( "
            "    SELECT "
            "        r.referee_id, "
            "        r.name as referee_name, "
            "        c.name as country, "
            "        COUNT(DISTINCT m.match_id) as total_matches, "
            "        COUNT(DISTINCT cat.category_id) as categories_count, "
            "        string_agg(DISTINCT cat.name, ', ') as categories, "
            "        COUNT(DISTINCT CASE WHEN m.round = 'Final' THEN m.match_id END) as finals_count, "
            "        MIN(m.date) as first_match, "
            "        MAX(m.date) as last_match "
            "    FROM Referees r "
            "    JOIN Countries c ON r.country_id = c.country_id "
            "    JOIN Matches m ON r.referee_id = m.referee_id "
            "    JOIN Categories cat ON m.category_id = cat.category_id "
            "    JOIN TournamentEditions te ON m.edition_id = te.edition_id "
            "    JOIN Tournaments t ON te.tournament_id = t.tournament_id "
            "    WHERE t.tournament_id = $1 AND te.year = $2 "
            "    GROUP BY r.referee_id, r.name, c.name "
            "    ORDER BY total_matches DESC, referee_name "
            ") "
            "SELECT *, "
            "    CASE "
            "        WHEN finals_count > 0 THEN '⭐ Arbitró final(es)' "
            "        ELSE '' "
            "    END as special_note "
            "FROM ArbitroStats;";

        result R = W.exec_params(query, tournament_id, year);

        if (R.empty()) {
            cout << "\n\033[1;31mNo se encontraron árbitros para el torneo y año especificados.\033[0m" << endl;
            esperarEntrada();
            return;
        }

        // Obtener nombre del torneo
        string tournament_name = W.exec(
            "SELECT name FROM Tournaments WHERE tournament_id = " + 
            to_string(tournament_id) + ";"
        )[0][0].as<string>();

        cout << "\n\033[1;36mÁrbitros de " << tournament_name << " " << year << "\033[0m\n" << endl;
        cout << "Total de árbitros: " << R.size() << "\n" << endl;

        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "\n\033[1;32m▶ " << row["referee_name"].as<string>() << "\033[0m" << endl;
            cout << "  🌍 País: " << row["country"].as<string>() << endl;
            cout << "  📊 Partidos arbitrados: " << row["total_matches"].as<int>() << endl;
            cout << "  🎾 Categorías: " << row["categories"].as<string>() << endl;
            cout << "  📅 Primer partido: " << row["first_match"].as<string>() << endl;
            cout << "  📅 Último partido: " << row["last_match"].as<string>() << endl;
            
            string special_note = row["special_note"].as<string>();
            if (!special_note.empty()) {
                cout << "  " << special_note << endl;
            }
            
            // Obtener detalles de los partidos
            result R_matches = W.exec_params(
                "SELECT m.date, m.round, cat.name as category, "
                "string_agg(DISTINCT CASE WHEN mp.team_number = 1 THEN p.name END, ' / ') as team1, "
                "string_agg(DISTINCT CASE WHEN mp.team_number = 2 THEN p.name END, ' / ') as team2 "
                "FROM Matches m "
                "JOIN Categories cat ON m.category_id = cat.category_id "
                "JOIN MatchParticipants mp ON m.match_id = mp.match_id "
                "JOIN Players p ON mp.player_id = p.player_id "
                "JOIN TournamentEditions te ON m.edition_id = te.edition_id "
                "WHERE m.referee_id = $1 "
                "AND te.tournament_id = $2 "
                "AND te.year = $3 "
                "GROUP BY m.date, m.round, cat.name "
                "ORDER BY m.date;",
                row["referee_id"].as<int>(), tournament_id, year
            );

            cout << "\n  📋 Detalle de partidos:" << endl;
            for (result::const_iterator match = R_matches.begin(); match != R_matches.end(); ++match) {
                cout << "    • " << match["date"].as<string>() 
                     << " | " << match["category"].as<string>()
                     << " | " << match["round"].as<string>() << endl;
                cout << "      " << match["team1"].as<string>() 
                     << " vs " << match["team2"].as<string>() << endl;
            }
            
            cout << string(50, '-') << endl;
        }

    } catch (const exception &e) {
        cerr << "\n\033[1;31mError al consultar árbitros: " << e.what() << "\033[0m" << endl;
    }
    esperarEntrada();
}

void consultarGananciasJugador(connection &c) {
    limpiarPantalla();
    try {
        work W(c);
        cout << "\033[1;33m╔═══════════════════════════════════════╗\033[0m" << endl;
        cout << "\033[1;33m║    CONSULTA DE GANANCIAS JUGADOR      ║\033[0m" << endl;
        cout << "\033[1;33m╚═══════════════════════════════════════╝\033[0m" << endl;

        // Primero seleccionar jugador
        result R_players = W.exec(
            "SELECT p.player_id, p.name, "
            "COALESCE(string_agg(DISTINCT c.name, ', '), 'Sin nacionalidad') as nationalities "
            "FROM Players p "
            "LEFT JOIN PlayerNationalities pn ON p.player_id = pn.player_id "
            "LEFT JOIN Countries c ON pn.country_id = c.country_id "
            "GROUP BY p.player_id, p.name "
            "ORDER BY p.name;"
        );

        if (R_players.empty()) {
            cout << "\n\033[1;31mNo hay jugadores registrados.\033[0m" << endl;
            esperarEntrada();
            return;
        }

        cout << "\nJugadores disponibles:\n" << string(70, '-') << endl;
        for (result::const_iterator row = R_players.begin(); row != R_players.end(); ++row) {
            cout << "ID: " << row["player_id"].as<int>() 
                 << " | " << row["name"].as<string>() 
                 << " [" << row["nationalities"].as<string>() << "]" << endl;
        }
        cout << string(70, '-') << endl;

        int player_id;
        cout << "\nSeleccione ID del jugador: ";
        cin >> player_id;
        limpiarEntrada();

        // Obtener torneos en los que participó el jugador
        result R_tournaments = W.exec(
            "WITH EdicionesJugador AS ("
            "    SELECT DISTINCT te.tournament_id, te.year "
            "    FROM TournamentEditions te "
            "    JOIN Matches m ON te.edition_id = m.edition_id "
            "    JOIN MatchParticipants mp ON m.match_id = mp.match_id "
            "    WHERE mp.player_id = " + to_string(player_id) + " "
            ")"
            "SELECT t.tournament_id, t.name, e.year "
            "FROM Tournaments t "
            "JOIN EdicionesJugador e ON t.tournament_id = e.tournament_id "
            "ORDER BY e.year DESC, t.name;"
        );

        if (R_tournaments.empty()) {
            cout << "\n\033[1;31mEl jugador no ha participado en ningún torneo.\033[0m" << endl;
            esperarEntrada();
            return;
        }

        cout << "\nTorneos disponibles:\n" << string(50, '-') << endl;
        string current_tournament = "";
        for (result::const_iterator row = R_tournaments.begin(); row != R_tournaments.end(); ++row) {
            string tournament_name = row["name"].as<string>();
            if (tournament_name != current_tournament) {
                current_tournament = tournament_name;
                cout << "\n" << tournament_name << ":" << endl;
            }
            cout << "  - " << row["year"].as<string>() 
                 << " (ID: " << row["tournament_id"].as<int>() << ")" << endl;
        }
        cout << string(50, '-') << endl;

        int tournament_id;
        cout << "\nSeleccione ID del torneo: ";
        cin >> tournament_id;
        limpiarEntrada();

        int year;
        cout << "Ingrese el año: ";
        cin >> year;
        limpiarEntrada();

        string query = 
            "WITH PlayerEarnings AS ( "
            "SELECT "
            "    m.match_id, "
            "    cat.name as category, "
            "    m.round, "
            "    m.date, "
            "    CASE WHEN mp.is_winner THEN m.winner_prize ELSE m.loser_prize END as prize, "
            "    mp.is_winner, "
            "    string_agg(DISTINCT CASE "
            "        WHEN mp2.team_number = mp.team_number THEN p2.name "
            "        END, ' / ') as teammate, "
            "    string_agg(DISTINCT CASE "
            "        WHEN mp2.team_number != mp.team_number THEN p2.name "
            "        END, ' / ') as opponents, "            "        mp.score "
            "    FROM Matches m "
            "    JOIN Categories cat ON m.category_id = cat.category_id "
            "    JOIN TournamentEditions te ON m.edition_id = te.edition_id "
            "    JOIN MatchParticipants mp ON m.match_id = mp.match_id "
            "    JOIN MatchParticipants mp2 ON m.match_id = mp2.match_id "
            "    JOIN Players p2 ON mp2.player_id = p2.player_id "
            "    WHERE te.tournament_id = $1 "
            "    AND te.year = $2 "
            "    AND mp.player_id = $3 "
            "    AND p2.player_id != $3 "
            "    GROUP BY m.match_id, cat.name, m.round, m.date, mp.is_winner, mp.score, "
            "             m.winner_prize, m.loser_prize "
            "    ORDER BY m.date "
            ") "
            "SELECT *, "
            "    SUM(prize) OVER () as total_earnings, "
            "    COUNT(*) OVER () as total_matches, "
            "    SUM(CASE WHEN is_winner THEN 1 ELSE 0 END) OVER () as matches_won "
            "FROM PlayerEarnings;";

        result R = W.exec_params(query, tournament_id, year, player_id);

        if (R.empty()) {
            cout << "\n\033[1;31mNo se encontraron participaciones del jugador en el torneo y año especificados.\033[0m" << endl;
            esperarEntrada();
            return;
        }

        // Obtener nombres
        string player_name = W.exec(
            "SELECT name FROM Players WHERE player_id = " + to_string(player_id) + ";"
        )[0][0].as<string>();

        string tournament_name = W.exec(
            "SELECT name FROM Tournaments WHERE tournament_id = " + to_string(tournament_id) + ";"
        )[0][0].as<string>();

        // Mostrar resumen
        double total_earnings = R[0]["total_earnings"].as<double>();
        int total_matches = R[0]["total_matches"].as<int>();
        int matches_won = R[0]["matches_won"].as<int>();

        cout << "\n\033[1;36mResumen de " << player_name << "\033[0m" << endl;
        cout << "Torneo: " << tournament_name << " " << year << endl;
        cout << string(50, '-') << endl;
        cout << "🏆 Total ganado: $" << fixed << setprecision(2) << total_earnings << endl;
        cout << "📊 Partidos jugados: " << total_matches << endl;
        cout << "✅ Partidos ganados: " << matches_won << endl;
        cout << "📈 Porcentaje victoria: " << fixed << setprecision(1) 
             << (total_matches > 0 ? (matches_won * 100.0 / total_matches) : 0) << "%" << endl;

        cout << "\n\033[1;32mDetalle de partidos:\033[0m\n" << endl;
        string current_category = "";

        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            string category = row["category"].as<string>();
            
            if (category != current_category) {
                cout << "\n\033[1;33m=== " << category << " ===\033[0m" << endl;
                current_category = category;
            }

            cout << "\n📅 " << row["date"].as<string>() 
                 << " | " << row["round"].as<string>() << endl;
            
            if (row["teammate"].is_null()) {
                cout << "🆚 vs " << row["opponents"].as<string>() << endl;
            } else {
                cout << "👥 Compañero: " << row["teammate"].as<string>() << endl;
                cout << "🆚 vs " << row["opponents"].as<string>() << endl;
            }

            cout << "🎯 Resultado: " << row["score"].as<string>() << " - " 
                 << (row["is_winner"].as<bool>() ? "\033[1;32mVictoria\033[0m" : "\033[1;31mDerrota\033[0m") << endl;
            cout << "💰 Premio: $" << fixed << setprecision(2) << row["prize"].as<double>() << endl;
            cout << string(40, '-') << endl;
        }

    } catch (const exception &e) {
        cerr << "\n\033[1;31mError al consultar ganancias: " << e.what() << "\033[0m" << endl;
    }
    esperarEntrada();
}

void registrarPartido(connection &c) {
    limpiarPantalla();
    try {
        work W(c);
        cout << "\033[1;33m╔═══════════════════════════════════════╗\033[0m" << endl;
        cout << "\033[1;33m║        REGISTRO DE NUEVO PARTIDO      ║\033[0m" << endl;
        cout << "\033[1;33m╚═══════════════════════════════════════╝\033[0m" << endl;

        // Selección de torneo y edición
        result R_editions = W.exec(
            "SELECT te.edition_id, t.name, te.year, te.start_date, te.end_date "
            "FROM TournamentEditions te "
            "JOIN Tournaments t ON te.tournament_id = t.tournament_id "
            "WHERE te.end_date >= CURRENT_DATE "
            "ORDER BY te.start_date ASC, t.name;"
        );

        if (R_editions.empty()) {
            cout << "\n\033[1;31mError: No hay ediciones de torneos futuras o en curso disponibles.\033[0m" << endl;
            esperarEntrada();
            return;
        }

        cout << "\nEdiciones de torneos disponibles:" << endl;
        cout << string(50, '-') << endl;
        for (result::const_iterator row = R_editions.begin(); row != R_editions.end(); ++row) {
            cout << "ID: " << row["edition_id"].as<int>() 
                 << " | " << row["name"].as<string>() 
                 << " " << row["year"].as<string>() 
                 << " (" << row["start_date"].as<string>() 
                 << " al " << row["end_date"].as<string>() << ")" << endl;
        }
        cout << string(50, '-') << endl;

        int edition_id;
        string start_date, end_date;
        while (true) {
            cout << "\nSeleccione ID de edición: ";
            if (cin >> edition_id) {
                bool edicion_valida = false;
                for (result::const_iterator row = R_editions.begin(); row != R_editions.end(); ++row) {
                    if (row["edition_id"].as<int>() == edition_id) {
                        edicion_valida = true;
                        start_date = row["start_date"].as<string>();
                        end_date = row["end_date"].as<string>();
                        break;
                    }
                }
                if (edicion_valida) break;
            }
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "\033[1;31mID de edición no válido.\033[0m" << endl;
        }
        limpiarEntrada();

        // Selección de categoría
        result R_categories = W.exec(
            "SELECT c.category_id, c.name, cc.allowed_gender, cc.players_per_team "
            "FROM Categories c "
            "JOIN CategoryConfigs cc ON c.category_id = cc.category_id "
            "ORDER BY c.name;"
        );

        cout << "\nCategorías disponibles:" << endl;
        cout << string(50, '-') << endl;
        for (result::const_iterator row = R_categories.begin(); row != R_categories.end(); ++row) {
            cout << "ID: " << row["category_id"].as<int>() 
                 << " | " << row["name"].as<string>() 
                 << " (" << row["players_per_team"].as<int>() << " jugador(es) por equipo)" << endl;
        }
        cout << string(50, '-') << endl;

        int category_id;
        int players_per_team;
        string categoria_nombre;
        char allowed_gender;
        while (true) {
            cout << "\nSeleccione ID de categoría: ";
            if (cin >> category_id) {
                bool categoria_valida = false;
                for (result::const_iterator row = R_categories.begin(); row != R_categories.end(); ++row) {
                    if (row["category_id"].as<int>() == category_id) {
                        categoria_valida = true;
                        players_per_team = row["players_per_team"].as<int>();
                        categoria_nombre = row["name"].as<string>();
                        allowed_gender = row["allowed_gender"].as<string>()[0];
                        break;
                    }
                }
                if (categoria_valida) break;
            }
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "\033[1;31mID de categoría no válido.\033[0m" << endl;
        }
        limpiarEntrada();

        // Vector de rondas ordenadas para validar la progresión
        vector<string> rondas_ordenadas = {
            "Primera Ronda", "Segunda Ronda", "Tercera Ronda",
            "Octavos", "Cuartos", "Semifinal", "Final"
        };

        // Verificar rondas ya jugadas en esta edición y categoría
        result R_rounds = W.exec(
            "SELECT DISTINCT round FROM Matches "
            "WHERE edition_id = " + to_string(edition_id) + " "
            "AND category_id = " + to_string(category_id) + ";"
        );

        vector<string> rondas_disponibles;
        if (R_rounds.empty()) {
            // Si no hay partidos registrados, solo permitir Primera Ronda
            rondas_disponibles.push_back("Primera Ronda");
        } else {
            // Encontrar la última ronda jugada
            string ultima_ronda;
            for (result::const_iterator row = R_rounds.begin(); row != R_rounds.end(); ++row) {
                ultima_ronda = row["round"].as<string>();
            }

            // Determinar siguiente ronda disponible
            bool found_last = false;
            for (const auto& ronda : rondas_ordenadas) {
                if (found_last) {
                    rondas_disponibles.push_back(ronda);
                    break;
                }
                if (ronda == ultima_ronda) {
                    found_last = true;
                }
            }
        }

        if (rondas_disponibles.empty()) {
            cout << "\n\033[1;31mError: Ya se han jugado todas las rondas para esta categoría en esta edición.\033[0m" << endl;
            esperarEntrada();
            return;
        }

    // Mostrar rondas disponibles
        cout << "\nRondas disponibles:" << endl;
        cout << string(50, '-') << endl;
        for (const auto& ronda : rondas_disponibles) {
            cout << "- " << ronda << endl;
        }
        cout << string(50, '-') << endl;

        string ronda;
        bool ronda_valida = false;
        while (!ronda_valida) {
            cout << "\nIngrese la ronda del torneo: ";
            getline(cin, ronda);
            for (const auto& r : rondas_disponibles) {
                if (r == ronda) {
                    ronda_valida = true;
                    break;
                }
            }
            if (!ronda_valida) {
                cout << "\033[1;31mRonda no válida. Seleccione una de las rondas listadas.\033[0m" << endl;
            }
        }

        // Fecha del partido
        string fecha;
        while (true) {
            cout << "Fecha del partido (YYYY-MM-DD): ";
            getline(cin, fecha);
            if (fecha.length() == 10 && fecha[4] == '-' && fecha[7] == '-' &&
                fecha >= start_date && fecha <= end_date) {
                // Verificar que la fecha sea válida
                try {
                    result R_date = W.exec(
                        "SELECT TO_DATE('" + fecha + "', 'YYYY-MM-DD') AS fecha;"
                    );
                    break;
                } catch (...) {
                    cout << "\033[1;31mFecha inválida.\033[0m" << endl;
                }
            }
            cout << "\033[1;31mFecha inválida o fuera del rango del torneo (" 
                 << start_date << " a " << end_date << ").\033[0m" << endl;
        }

        // Árbitro
        result R_referees = W.exec(
            "SELECT r.referee_id, r.name, c.name as country "
            "FROM Referees r "
            "JOIN Countries c ON r.country_id = c.country_id "
            "WHERE r.active = true "
            "ORDER BY r.name;"
        );
        
        if (R_referees.empty()) {
            cout << "\n\033[1;31mError: No hay árbitros activos registrados.\033[0m" << endl;
            esperarEntrada();
            return;
        }

        cout << "\nÁrbitros disponibles:" << endl;
        cout << string(50, '-') << endl;
        for (result::const_iterator row = R_referees.begin(); row != R_referees.end(); ++row) {
            cout << "ID: " << row["referee_id"].as<int>() 
                 << " | " << row["name"].as<string>() 
                 << " (" << row["country"].as<string>() << ")" << endl;
        }
        cout << string(50, '-') << endl;

        int referee_id;
        while (true) {
            cout << "\nSeleccione ID del árbitro: ";
            if (cin >> referee_id) {
                bool arbitro_valido = false;
                for (result::const_iterator row = R_referees.begin(); row != R_referees.end(); ++row) {
                    if (row["referee_id"].as<int>() == referee_id) {
                        arbitro_valido = true;
                        break;
                    }
                }
                if (arbitro_valido) break;
            }
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "\033[1;31mID de árbitro no válido.\033[0m" << endl;
        }
        limpiarEntrada();

        // Obtener premios para la ronda
        double winner_prize, loser_prize;
        if (!obtenerPremios(W, edition_id, ronda, winner_prize, loser_prize)) {
            cout << "\n\033[1;31mError al obtener los premios para la ronda.\033[0m" << endl;
            esperarEntrada();
            return;
        }

        cout << "\nPremios establecidos:" << endl;
        cout << "Ganador: $" << fixed << setprecision(2) << winner_prize << endl;
        cout << "Perdedor: $" << fixed << setprecision(2) << loser_prize << endl;

        // Insertar el partido
        result R_match = W.exec(
            "INSERT INTO Matches "
            "(edition_id, category_id, round, date, referee_id, winner_prize, loser_prize) "
            "VALUES (" + 
            to_string(edition_id) + ", " + 
            to_string(category_id) + ", '" + 
            ronda + "', '" + 
            fecha + "', " + 
            to_string(referee_id) + ", " + 
            to_string(winner_prize) + ", " + 
            to_string(loser_prize) + ") RETURNING match_id;"
        );
        
        int match_id = R_match[0][0].as<int>();

        // Vectores para almacenar los IDs de los jugadores
        vector<int> jugadores_equipo1;
        vector<int> jugadores_equipo2;

        // Registrar jugadores por equipo
        for (int team = 1; team <= 2; team++) {
            cout << "\n=== Equipo " << team << " ===" << endl;
            
            while (jugadores_equipo1.size() < players_per_team || 
                   jugadores_equipo2.size() < players_per_team) {
                
                // Obtener jugadores disponibles según género
                string query_players = 
                    "SELECT p.player_id, p.name, p.gender, string_agg(c.name, ', ') as countries "
                    "FROM Players p "
                    "LEFT JOIN PlayerNationalities pn ON p.player_id = pn.player_id "
                    "LEFT JOIN Countries c ON pn.country_id = c.country_id ";
                
                if (allowed_gender != 'B') {
                    query_players += "WHERE p.gender = '" + string(1, allowed_gender) + "' ";
                }
                
                query_players += "GROUP BY p.player_id, p.name, p.gender ORDER BY p.name;";
                
                result R_players = W.exec(query_players);

                cout << "\nJugadores disponibles:" << endl;
                cout << string(70, '-') << endl;
                for (result::const_iterator row = R_players.begin(); row != R_players.end(); ++row) {
                    cout << "ID: " << row["player_id"].as<int>() 
                         << " | " << row["name"].as<string>() 
                         << " (" << row["gender"].as<string>() << ") "
                         << "[" << row["countries"].as<string>() << "]" << endl;
                }
                cout << string(70, '-') << endl;

                int player_id;
                while (true) {
                    if (categoria_nombre.find("Dobles") != string::npos) {
                        cout << "\nSeleccione ID del jugador " 
                             << (team == 1 ? jugadores_equipo1.size() + 1 : jugadores_equipo2.size() + 1) 
                             << "/" << players_per_team << ": ";
                    } else {
                        cout << "\nSeleccione ID del jugador: ";
                    }

                    if (cin >> player_id) {
                        bool jugador_valido = false;
                        bool jugador_repetido = false;
                        char genero_jugador;

                        // Verificar si el jugador existe y obtener su género
                        for (result::const_iterator row = R_players.begin(); row != R_players.end(); ++row) {
                            if (row["player_id"].as<int>() == player_id) {
                                jugador_valido = true;
                                genero_jugador = row["gender"].as<string>()[0];
                                break;
                            }
                        }

                        // Verificar si el jugador ya fue seleccionado
                        for (int id : jugadores_equipo1) {
                            if (id == player_id) jugador_repetido = true;
                        }
                        for (int id : jugadores_equipo2) {
                            if (id == player_id) jugador_repetido = true;
                        }

                        // Verificar si el jugador tiene otro partido en la misma fecha
                        if (tienePartidoEnFecha(W, player_id, fecha)) {
                            cout << "\033[1;31mEl jugador ya tiene un partido programado para esta fecha.\033[0m" << endl;
                            continue;
                        }

                        if (jugador_valido && !jugador_repetido) {
                            // Validaciones específicas para dobles mixtos
                            if (categoria_nombre == "Dobles mixtos") {
                                if (team == 1) {
                                    if (jugadores_equipo1.empty() || 
                                        obtenerGeneroJugador(W, jugadores_equipo1[0]) != genero_jugador) {
                                        jugadores_equipo1.push_back(player_id);
                                        break;
                                    } else {
                                        cout << "\033[1;31mEn dobles mixtos, los jugadores del mismo equipo deben ser de distinto género.\033[0m" << endl;
                                    }
                                } else {
                                    if (jugadores_equipo2.empty() || 
                                        obtenerGeneroJugador(W, jugadores_equipo2[0]) != genero_jugador) {
                                        jugadores_equipo2.push_back(player_id);
                                        break;
                                    } else {
                                        cout << "\033[1;31mEn dobles mixtos, los jugadores del mismo equipo deben ser de distinto género.\033[0m" << endl;
                                    }
                                }
                            } else {
                                // Para otras categorías
                                if (team == 1) {
                                    jugadores_equipo1.push_back(player_id);
                                } else {
                                    jugadores_equipo2.push_back(player_id);
                                }
                                break;
                            }
                        } else if (jugador_repetido) {
                            cout << "\033[1;31mEste jugador ya fue seleccionado.\033[0m" << endl;
                        }
                    }
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    cout << "\033[1;31mSelección no válida.\033[0m" << endl;
                }
                limpiarEntrada();
            }
        }

        // Validar la composición de ambos equipos
        if (!validarEquipo(W, jugadores_equipo1, categoria_nombre) || 
            !validarEquipo(W, jugadores_equipo2, categoria_nombre)) {
            cout << "\n\033[1;31mError: La composición de los equipos no cumple con los requisitos de la categoría.\033[0m" << endl;
            esperarEntrada();
            return;
        }

        // Registrar resultado
        int winning_team;
        while (true) {
            cout << "\n¿Qué equipo ganó? (1/2): ";
            if (cin >> winning_team && (winning_team == 1 || winning_team == 2)) {
                break;
            }
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "\033[1;31mPor favor, ingrese 1 o 2.\033[0m" << endl;
        }
        limpiarEntrada();

        // Registrar score
        string score;
        bool score_valido = false;
        while (!score_valido) {
            cout << "Ingrese el score (formato: 6-4,7-5,6-4): ";
            getline(cin, score);
            
            // Validación básica del formato del score
            bool formato_correcto = true;
            for (char c : score) {
                if (!isdigit(c) && c != '-' && c != ',') {
                    formato_correcto = false;
                    break;
                }
            }
            if (formato_correcto) score_valido = true;
            else cout << "\033[1;31mFormato de score inválido. Use el formato: 6-4,7-5,6-4\033[0m" << endl;
        }

        // Registrar participantes
        for (int player_id : jugadores_equipo1) {
            W.exec(
                "INSERT INTO MatchParticipants (match_id, player_id, team_number, is_winner, score) "
                "VALUES (" + to_string(match_id) + ", " + to_string(player_id) + 
                ", 1, " + (winning_team == 1 ? "true" : "false") + ", '" + score + "');"
            );
        }

        for (int player_id : jugadores_equipo2) {
            W.exec(
                "INSERT INTO MatchParticipants (match_id, player_id, team_number, is_winner, score) "
                "VALUES (" + to_string(match_id) + ", " + to_string(player_id) + 
                ", 2, " + (winning_team == 2 ? "true" : "false") + ", '" + score + "');"
            );
        }

        W.commit();
        cout << "\n\033[1;32mPartido registrado exitosamente.\033[0m" << endl;

    } catch (const exception &e) {
        cerr << "\n\033[1;31mError durante el registro: " << e.what() << "\033[0m" << endl;
    }
    esperarEntrada();
}

void registrarJugador(connection &c) {
    limpiarPantalla();
    try {
        work W(c);
        cout << "\033[1;33m╔═══════════════════════════════════════╗\033[0m" << endl;
        cout << "\033[1;33m║        REGISTRO DE NUEVO JUGADOR      ║\033[0m" << endl;
        cout << "\033[1;33m╚═══════════════════════════════════════╝\033[0m" << endl;

        string nombre, fecha_nacimiento;
        char genero;
        
        // Validación del nombre
        while (true) {
            cout << "\nNombre del jugador: ";
            getline(cin, nombre);
            if (nombre.length() >= 3 && nombre.length() <= 100) {
                break;
            }
            cout << "El nombre debe tener entre 3 y 100 caracteres." << endl;
        }

        // Validación del género
        while (true) {
            cout << "Género (M/F): ";
            cin >> genero;
            genero = toupper(genero);
            if (genero == 'M' || genero == 'F') {
                break;
            }
            cout << "Por favor, ingrese M para masculino o F para femenino." << endl;
            limpiarEntrada();
        }
        limpiarEntrada();

        // Validación de la fecha de nacimiento
        bool fecha_valida = false;
        while (!fecha_valida) {
            cout << "Fecha de nacimiento (YYYY-MM-DD): ";
            getline(cin, fecha_nacimiento);
            
            if (fecha_nacimiento.length() == 10 && 
                fecha_nacimiento[4] == '-' && 
                fecha_nacimiento[7] == '-') {
                try {
                    // Verificar que la fecha sea válida usando una consulta
                    result R = W.exec(
                        "SELECT TO_DATE('" + fecha_nacimiento + "', 'YYYY-MM-DD') <= CURRENT_DATE "
                        "AS is_valid;"
                    );
                    if (R[0][0].as<bool>()) {
                        fecha_valida = true;
                    } else {
                        cout << "La fecha debe ser anterior o igual a la fecha actual." << endl;
                    }
                } catch (...) {
                    cout << "Formato de fecha inválido. Use YYYY-MM-DD." << endl;
                }
            } else {
                cout << "Formato de fecha inválido. Use YYYY-MM-DD." << endl;
            }
        }

        // Verificar si el jugador ya existe
        result R_check = W.exec(
            "SELECT COUNT(*) FROM Players WHERE name = '" + nombre + "';"
        );
        if (R_check[0][0].as<int>() > 0) {
            cout << "\nError: Ya existe un jugador con ese nombre." << endl;
            esperarEntrada();
            return;
        }

        // Insertar jugador
        result R = W.exec(
            "INSERT INTO Players (name, gender, birth_date) VALUES ('" +
            nombre + "', '" + genero + "', '" + fecha_nacimiento + 
            "') RETURNING player_id;"
        );
        int player_id = R[0][0].as<int>();

        // Agregar nacionalidades
        result R_countries = W.exec("SELECT country_id, name FROM Countries ORDER BY name;");
        if (R_countries.empty()) {
            cout << "Error: No hay países registrados en el sistema." << endl;
            esperarEntrada();
            return;
        }

        char mas_nacionalidades;
        bool primera_nacionalidad = true;
        do {
            cout << "\nPaíses disponibles:" << endl;
            for (result::const_iterator row = R_countries.begin(); row != R_countries.end(); ++row) {
                cout << row[0].as<int>() << ". " << row[1].as<string>() << endl;
            }

            int country_id;
            bool id_valido = false;
            while (!id_valido) {
                cout << "\nSeleccione ID del país: ";
                if (cin >> country_id) {
                    for (result::const_iterator row = R_countries.begin(); row != R_countries.end(); ++row) {
                        if (row[0].as<int>() == country_id) {
                            id_valido = true;
                            break;
                        }
                    }
                }
                if (!id_valido) {
                    cout << "ID de país no válido. Intente nuevamente." << endl;
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                }
            }
            limpiarEntrada();

            try {
                W.exec(
                    "INSERT INTO PlayerNationalities (player_id, country_id) VALUES (" +
                    to_string(player_id) + ", " + to_string(country_id) + ");"
                );
            } catch (...) {
                cout << "Esta nacionalidad ya fue registrada para este jugador." << endl;
            }

            if (primera_nacionalidad) {
                primera_nacionalidad = false;
                cout << "¿Agregar otra nacionalidad? (s/n): ";
                cin >> mas_nacionalidades;
                limpiarEntrada();
            } else {
                cout << "¿Agregar otra nacionalidad más? (s/n): ";
                cin >> mas_nacionalidades;
                limpiarEntrada();
            }
        } while (mas_nacionalidades == 's' || mas_nacionalidades == 'S');

        W.commit();
        cout << "\n\033[1;32mJugador registrado exitosamente.\033[0m" << endl;

    } catch (const exception &e) {
        cerr << "\n\033[1;31mError durante el registro: " << e.what() << "\033[0m" << endl;
    }
    esperarEntrada();
}

void registrarTorneo(connection &c) {
    limpiarPantalla();
    try {
        work W(c);
        cout << "\033[1;33m╔═══════════════════════════════════════╗\033[0m" << endl;
        cout << "\033[1;33m║     REGISTRO DE NUEVA EDICIÓN         ║\033[0m" << endl;
        cout << "\033[1;33m╚═══════════════════════════════════════╝\033[0m" << endl;

        // Seleccionar torneo
        result R_tournaments = W.exec("SELECT tournament_id, name FROM Tournaments ORDER BY name;");
        cout << "\nTorneos disponibles:" << endl;
        for (result::const_iterator row = R_tournaments.begin(); row != R_tournaments.end(); ++row) {
            cout << row[0].as<int>() << ". " << row[1].as<string>() << endl;
        }

        int tournament_id;
        cout << "\nSeleccione ID del torneo: ";
        cin >> tournament_id;
        limpiarEntrada();

        // Año
        int year;
        cout << "Año de la edición: ";
        cin >> year;
        limpiarEntrada();

        // Sede
        result R_venues = W.exec("SELECT venue_id, name FROM Venues ORDER BY name;");
        cout << "\nSedes disponibles:" << endl;
        for (result::const_iterator row = R_venues.begin(); row != R_venues.end(); ++row) {
            cout << row[0].as<int>() << ". " << row[1].as<string>() << endl;
        }

        int venue_id;
        cout << "\nSeleccione ID de la sede: ";
        cin >> venue_id;
        limpiarEntrada();

        // Fechas
        string fecha_inicio, fecha_fin;
        cout << "Fecha de inicio (YYYY-MM-DD): ";
        getline(cin, fecha_inicio);
        cout << "Fecha de finalización (YYYY-MM-DD): ";
        getline(cin, fecha_fin);

        // Insertar edición
        W.exec_params(
            "INSERT INTO TournamentEditions (tournament_id, year, venue_id, start_date, end_date) "
            "VALUES ($1, $2, $3, $4, $5);",
            tournament_id, year, venue_id, fecha_inicio, fecha_fin
        );

        W.commit();
        cout << "\nEdición del torneo registrada exitosamente." << endl;

    } catch (const exception &e) {
        cerr << "\nError durante el registro: " << e.what() << endl;
    }
    esperarEntrada();
}

void menuPrincipal(connection &c) {
    int opcion;
    do {
        limpiarPantalla();
        cout << "\033[1;34m┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓\033[0m\n";
        cout << "\033[1;34m┃        GRAND SLAM TENNIS SYSTEM      ┃\033[0m\n";
        cout << "\033[1;34m┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\033[0m\n";
        cout << "\033[1;34m┃\033[0m  \033[1;33m== Consultas Principales ==\033[0m         \033[1;34m┃\033[0m\n";
        cout << "\033[1;34m┃\033[0m  \033[1;32m1.\033[0m 🎯 Partidos por Torneo y Año     \033[1;34m┃\033[0m\n";
        cout << "\033[1;34m┃\033[0m  \033[1;32m2.\033[0m 👨‍⚖️ Árbitros por Torneo          \033[1;34m┃\033[0m\n";
        cout << "\033[1;34m┃\033[0m  \033[1;32m3.\033[0m 💰 Ganancias por Jugador         \033[1;34m┃\033[0m\n";
        cout << "\033[1;34m┃\033[0m  \033[1;32m4.\033[0m 👨‍🏫 Historial de Entrenadores   \033[1;34m┃\033[0m\n";
        cout << "\033[1;34m┃\033[0m                                      \033[1;34m┃\033[0m\n";
        cout << "\033[1;34m┃\033[0m  \033[1;33m== Consultas Generales ==\033[0m           \033[1;34m┃\033[0m\n";
        cout << "\033[1;34m┃\033[0m  \033[1;32m5.\033[0m 🏆 Consultar Torneos             \033[1;34m┃\033[0m\n";
        cout << "\033[1;34m┃\033[0m  \033[1;32m6.\033[0m 👥 Directorio de Jugadores       \033[1;34m┃\033[0m\n";
        cout << "\033[1;34m┃\033[0m  \033[1;32m7.\033[0m 🎽 Directorio de Árbitros        \033[1;34m┃\033[0m\n";
        cout << "\033[1;34m┃\033[0m                                      \033[1;34m┃\033[0m\n";
        cout << "\033[1;34m┃\033[0m  \033[1;33m== Gestión de Datos ==\033[0m              \033[1;34m┃\033[0m\n";
        cout << "\033[1;34m┃\033[0m  \033[1;32m8.\033[0m 📝 Registrar Partido             \033[1;34m┃\033[0m\n";
        cout << "\033[1;34m┃\033[0m  \033[1;32m9.\033[0m 👤 Registrar Jugador             \033[1;34m┃\033[0m\n";
        cout << "\033[1;34m┃\033[0m  \033[1;32m10.\033[0m 🎾 Registrar Torneo             \033[1;34m┃\033[0m\n";
        cout << "\033[1;34m┃\033[0m                                      \033[1;34m┃\033[0m\n";
        cout << "\033[1;34m┃\033[0m  \033[1;31m0.\033[0m 🚪 Salir del Sistema             \033[1;34m┃\033[0m\n";
        cout << "\033[1;34m┃\033[0m                                      \033[1;34m┃\033[0m\n";
        cout << "\033[1;34m┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\033[0m\n";
        cout << "\n\033[1;37m▶ Seleccione una opción:\033[0m ";

        if (!(cin >> opcion)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            opcion = -1;
        } else {
            limpiarEntrada();
        }

        try {
            switch (opcion) {
                case 1:
                    consultarPartidosPorTorneo(c);
                    break;
                case 2:
                    consultarArbitrosPorTorneo(c);
                    break;
                case 3:
                    consultarGananciasJugador(c);
                    break;
                case 4:
                    consultarHistorialEntrenadores(c);
                    break;
                case 5:
                    consultarTorneos(c);
                    break;
                case 6:
                    consultarJugadores(c);
                    break;
                case 7:
                    consultarArbitros(c);
                    break;
                case 8:
                    registrarPartido(c);
                    break;
                case 9:
                    registrarJugador(c);
                    break;
                case 10:
                    registrarTorneo(c);
                    break;
                case 0:
                    limpiarPantalla();
                    cout << "\033[1;34m┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓\033[0m\n";
                    cout << "\033[1;34m┃       GRAND SLAM TENNIS SYSTEM     ┃\033[0m\n";
                    cout << "\033[1;34m┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\033[0m\n";
                    cout << "\033[1;34m┃\033[0m      Finalizando el sistema...     \033[1;34m┃\033[0m\n";
                    cout << "\033[1;34m┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\033[0m\n";
                    system("sleep 1");
                    break;
                default:
                    limpiarPantalla();
                    cout << "\033[1;31m┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓\033[0m\n";
                    cout << "\033[1;31m┃           ERROR DE ENTRADA         ┃\033[0m\n";
                    cout << "\033[1;31m┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\033[0m\n";
                    cout << "\033[1;31m┃    Por favor, seleccione una opción┃\033[0m\n";
                    cout << "\033[1;31m┃         válida (0-10)             ┃\033[0m\n";
                    cout << "\033[1;31m┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\033[0m\n";
                    esperarEntrada();
            }
        } catch (const exception &e) {
            cerr << "\n\033[1;31m[ERROR] Operación fallida: " << e.what() << "\033[0m" << endl;
            esperarEntrada();
        }
    } while (opcion != 0);
}

int main() {
    limpiarPantalla();
    
    cout << "\033[1;34m┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓\033[0m\n";
    cout << "\033[1;34m┃       GRAND SLAM TENNIS SYSTEM     ┃\033[0m\n";
    cout << "\033[1;34m┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\033[0m\n";
    cout << "\033[1;34m┃\033[0m     Iniciando el sistema...        \033[1;34m┃\033[0m\n";
    cout << "\033[1;34m┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\033[0m\n";
    
    try {
        cout << "\n  🔄 Conectando con la base de datos..." << endl;
        connection c = conectar();
        system("sleep 1");
        
        cout << "\n  🔍 Verificando estructura de datos..." << endl;
        comprobarTablas(c);
        system("sleep 1");
        
        cout << "\033[1;34m┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓\033[0m\n";
        cout << "\033[1;34m┃       GRAND SLAM TENNIS SYSTEM     ┃\033[0m\n";
        cout << "\033[1;34m┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\033[0m\n";
        cout << "\033[1;34m┃\033[0m  ✅ Sistema iniciado correctamente \033[1;34m┃\033[0m\n";
        cout << "\033[1;34m┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\033[0m\n";
        system("sleep 1");
        
        menuPrincipal(c);
        
    } catch (const exception &e) {
        cout << "\033[1;31m┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓\033[0m\n";
        cout << "\033[1;31m┃           ERROR CRÍTICO            ┃\033[0m\n";
        cout << "\033[1;31m┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\033[0m\n";
        cout << "\033[1;31m┃\033[0m Error: " << e.what() << "\033[1;31m┃\033[0m\n";
        cout << "\033[1;31m┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\033[0m\n";
        esperarEntrada();
        return 1;
    }
    
    cout << "\033[1;34m┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓\033[0m\n";
    cout << "\033[1;34m┃       GRAND SLAM TENNIS SYSTEM     ┃\033[0m\n";
    cout << "\033[1;34m┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\033[0m\n";
    cout << "\033[1;34m┃\033[0m   ¡Gracias por usar el sistema!    \033[1;34m┃\033[0m\n";
    cout << "\033[1;34m┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\033[0m\n";
    system("sleep 1");
    return 0;
}