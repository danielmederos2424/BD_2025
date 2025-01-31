#include <iostream>
#include <string>
#include <pqxx/pqxx>
#include <sstream>
#include <iomanip>
#include <vector>

using namespace std;
using namespace pqxx;

// Funciones auxiliares básicas
void limpiarPantalla() {
    system("clear");
}

void esperarEntrada() {
    cout << "\nPresione Enter para continuar...";
    cin.get();
}

// Validaciones esenciales
bool validarDNI(const string& str) {
    if(str.length() != 9) return false;
    for(size_t i = 0; i < 8; i++) {
        if(!isdigit(str[i])) return false;
    }
    return isalpha(str[8]);
}

bool validarMatricula(const string& str) {
    if(str.length() != 7) return false;
    for(size_t i = 0; i < 4; i++) {
        if(!isdigit(str[i])) return false;
    }
    for(size_t i = 4; i < 7; i++) {
        if(!isalpha(str[i])) return false;
    }
    return true;
}

// Función para obtener entrada numérica
template <typename T>
T obtenerNumero(const string& mensaje, T min, T max) {
    T numero;
    string entrada;
    while (true) {
        cout << mensaje;
        getline(cin, entrada);
        stringstream ss(entrada);
        if (ss >> numero && numero >= min && numero <= max) {
            return numero;
        }
        cout << "Error: Ingrese un número entre " << min << " y " << max << endl;
    }
}

// Función para obtener texto
string obtenerTexto(const string& mensaje, size_t minLen, size_t maxLen) {
    string texto;
    while (true) {
        cout << mensaje;
        getline(cin, texto);
        if (texto.length() >= minLen && texto.length() <= maxLen) {
            return texto;
        }
        cout << "Error: El texto debe tener entre " << minLen << " y " 
             << maxLen << " caracteres." << endl;
    }
}

// Función para mostrar lista de opciones y obtener selección
int mostrarOpciones(const vector<pair<int, string>>& opciones, 
                   const string& mensaje, 
                   bool permitirCancelar = true) {
    cout << "\n" << mensaje << ":" << endl;
    cout << string(40, '-') << endl;
    
    for (const auto& opcion : opciones) {
        cout << opcion.first << ". " << opcion.second << endl;
    }
    
    if (permitirCancelar) {
        cout << "0. Cancelar" << endl;
    }
    cout << string(40, '-') << endl;

    int max = opciones.empty() ? 0 : opciones.back().first;
    return obtenerNumero("Seleccione una opción: ", 
                        permitirCancelar ? 0 : 1, 
                        max);
}

// Función para obtener lista de opciones desde la base de datos
vector<pair<int, string>> obtenerOpciones(connection& c, 
                                        const string& query) {
    vector<pair<int, string>> opciones;
    work W(c);
    result R = W.exec(query);
    W.commit();

    for (const auto& row : R) {
        opciones.emplace_back(row[0].as<int>(), row[1].as<string>());
    }
    return opciones;
}

// Conexión a la base de datos
connection conectarBD() {
    const string CONFIG_DB =
        "dbname=taller "
        "user=postgres "
        "password=1234admin "
        "hostaddr=127.0.0.1 "
        "port=5432";
    try {
        connection c(CONFIG_DB);
        if (!c.is_open()) {
            throw runtime_error("No se pudo conectar a la base de datos");
        }
        return c;
    } catch (const exception& e) {
        throw runtime_error("Error de conexión: " + string(e.what()));
    }
}

// Inicialización de tablas
void crearTablas(connection &c) {
    try {
        work W(c);

        // Tabla Clientes
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS Clientes (
                dni VARCHAR(10) PRIMARY KEY,
                nombre VARCHAR(50) NOT NULL,
                apellidos VARCHAR(100) NOT NULL,
                direccion VARCHAR(200) NOT NULL,
                telefono VARCHAR(15) NOT NULL
            )
        )");

        // Tabla Vehículos
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS Vehiculos (
                matricula VARCHAR(10) PRIMARY KEY,
                modelo VARCHAR(100) NOT NULL,
                color VARCHAR(50) NOT NULL,
                dni_cliente VARCHAR(10) REFERENCES Clientes(dni)
            )
        )");

        // Tabla Mecánicos
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS Mecanicos (
                id_mecanico SERIAL PRIMARY KEY,
                nombre VARCHAR(50) NOT NULL,
                apellidos VARCHAR(100) NOT NULL,
                disponible BOOLEAN DEFAULT true
            )
        )");

        // Tabla Reparaciones
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS Reparaciones (
                id_reparacion SERIAL PRIMARY KEY,
                matricula_vehiculo VARCHAR(10) REFERENCES Vehiculos(matricula),
                fecha_entrada DATE NOT NULL,
                hora_entrada TIME NOT NULL,
                mecanico_principal INTEGER REFERENCES Mecanicos(id_mecanico),
                mano_obra DECIMAL(10,2) DEFAULT 0,
                estado VARCHAR(20) DEFAULT 'En evaluación',
                fecha_fin DATE
            )
        )");

        // Tabla Mecanicos_Reparacion
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS Mecanicos_Reparacion (
                id_reparacion INTEGER REFERENCES Reparaciones(id_reparacion),
                id_mecanico INTEGER REFERENCES Mecanicos(id_mecanico),
                PRIMARY KEY (id_reparacion, id_mecanico)
            )
        )");

        // Tabla Repuestos
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS Repuestos (
                id_repuesto SERIAL PRIMARY KEY,
                nombre VARCHAR(100) NOT NULL,
                precio_unidad DECIMAL(10,2) NOT NULL
            )
        )");

        // Tabla Repuestos_Reparacion
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS Repuestos_Reparacion (
                id_reparacion INTEGER REFERENCES Reparaciones(id_reparacion),
                id_repuesto INTEGER REFERENCES Repuestos(id_repuesto),
                cantidad INTEGER NOT NULL,
                PRIMARY KEY (id_reparacion, id_repuesto)
            )
        )");

        W.commit();
    } catch (const exception &e) {
        cerr << "Error durante la inicialización de tablas: " << e.what() << endl;
        throw;
    }
}

void registrarClienteVehiculo(connection &c) {
    limpiarPantalla();
    cout << "\n=== REGISTRO DE CLIENTE Y VEHÍCULO ===\n" << endl;

    try {
        // Datos del cliente
        string dni = obtenerTexto("DNI (8 números y 1 letra): ", 9, 9);
        if (!validarDNI(dni)) {
            cout << "Error: DNI inválido" << endl;
            esperarEntrada();
            return;
        }

        work W(c);
        result R = W.exec_params("SELECT dni FROM Clientes WHERE dni = $1", dni);
        
        if (!R.empty()) {
            cout << "El cliente ya está registrado" << endl;
            esperarEntrada();
            return;
        }

        string nombre = obtenerTexto("Nombre: ", 2, 50);
        string apellidos = obtenerTexto("Apellidos: ", 2, 100);
        string direccion = obtenerTexto("Dirección: ", 5, 200);
        string telefono = obtenerTexto("Teléfono: ", 9, 15);

        // Datos del vehículo
        string matricula = obtenerTexto("Matrícula (4 números y 3 letras): ", 7, 7);
        if (!validarMatricula(matricula)) {
            cout << "Error: Matrícula inválida" << endl;
            esperarEntrada();
            return;
        }

        R = W.exec_params("SELECT matricula FROM Vehiculos WHERE matricula = $1", matricula);
        if (!R.empty()) {
            cout << "El vehículo ya está registrado" << endl;
            esperarEntrada();
            return;
        }

        string modelo = obtenerTexto("Modelo: ", 2, 100);
        string color = obtenerTexto("Color: ", 2, 50);

        // Registrar cliente y vehículo
        W.exec_params(
            "INSERT INTO Clientes (dni, nombre, apellidos, direccion, telefono) "
            "VALUES ($1, $2, $3, $4, $5)",
            dni, nombre, apellidos, direccion, telefono
        );

        W.exec_params(
            "INSERT INTO Vehiculos (matricula, modelo, color, dni_cliente) "
            "VALUES ($1, $2, $3, $4)",
            matricula, modelo, color, dni
        );

        W.commit();
        cout << "\nCliente y vehículo registrados exitosamente" << endl;

    } catch (const exception &e) {
        cout << "\nError: " << e.what() << endl;
    }
    esperarEntrada();
}

void asignarMecanicos(connection &c) {
    limpiarPantalla();
    cout << "\n=== ASIGNACIÓN DE MECÁNICOS A REPARACIÓN ===\n" << endl;

    try {
        // Obtener y mostrar vehículos para selección
        vector<pair<string, string>> vehiculos;
        {
            work W(c);
            result R = W.exec(
                "SELECT v.matricula, v.modelo || ' (' || v.matricula || ') - ' || "
                "c.nombre || ' ' || c.apellidos "
                "FROM Vehiculos v "
                "JOIN Clientes c ON v.dni_cliente = c.dni "
                "ORDER BY v.matricula"
            );
            
            for (const auto &row : R) {
                vehiculos.emplace_back(
                    row[0].as<string>(), // matricula
                    row[1].as<string>()  // descripción
                );
            }
            W.commit();
        }

        if (vehiculos.empty()) {
            cout << "No hay vehículos registrados" << endl;
            esperarEntrada();
            return;
        }

        // Mostrar vehículos
        cout << "\nVehículos disponibles:" << endl;
        cout << string(40, '-') << endl;
        for (size_t i = 0; i < vehiculos.size(); ++i) {
            cout << (i + 1) << ". " << vehiculos[i].second << endl;
        }
        cout << "0. Cancelar" << endl;
        cout << string(40, '-') << endl;

        int seleccion = obtenerNumero("Seleccione una opción: ", 0, (int)vehiculos.size());
        if (seleccion == 0) return;

        string matricula = vehiculos[seleccion - 1].first;

        work W(c);
        // Verificar si hay una reparación activa
        result R = W.exec_params(
            "SELECT id_reparacion FROM Reparaciones "
            "WHERE matricula_vehiculo = $1 AND estado != 'Finalizada'",
            matricula
        );

        if (!R.empty()) {
            cout << "Este vehículo ya tiene una reparación activa" << endl;
            esperarEntrada();
            return;
        }

        // Obtener y mostrar mecánicos disponibles
        vector<pair<int, string>> mecanicos;
        R = W.exec(
            "SELECT id_mecanico, nombre || ' ' || apellidos "
            "FROM Mecanicos WHERE disponible = true "
            "ORDER BY id_mecanico"
        );

        for (const auto &row : R) {
            mecanicos.emplace_back(
                row[0].as<int>(),
                row[1].as<string>()
            );
        }

        if (mecanicos.empty()) {
            cout << "No hay mecánicos disponibles" << endl;
            esperarEntrada();
            return;
        }

        // Mostrar mecánicos
        cout << "\nMecánicos disponibles:" << endl;
        cout << string(40, '-') << endl;
        for (const auto &mec : mecanicos) {
            cout << mec.first << ". " << mec.second << endl;
        }
        cout << "0. Cancelar" << endl;
        cout << string(40, '-') << endl;

        int mecanico_principal = obtenerNumero("Seleccione el mecánico principal: ", 0, 999999);
        if (mecanico_principal == 0) return;

        // Verificar que el mecánico seleccionado existe
        bool encontrado = false;
        for (const auto &mec : mecanicos) {
            if (mec.first == mecanico_principal) {
                encontrado = true;
                break;
            }
        }
        if (!encontrado) {
            cout << "Mecánico no válido" << endl;
            esperarEntrada();
            return;
        }

        // Crear la reparación
        R = W.exec_params(
            "INSERT INTO Reparaciones "
            "(matricula_vehiculo, fecha_entrada, hora_entrada, mecanico_principal, estado) "
            "VALUES ($1, CURRENT_DATE, CURRENT_TIME, $2, 'En evaluación') "
            "RETURNING id_reparacion",
            matricula, mecanico_principal
        );

        int id_reparacion = R[0][0].as<int>();

        // Marcar mecánico principal como no disponible
        W.exec_params(
            "UPDATE Mecanicos SET disponible = false WHERE id_mecanico = $1",
            mecanico_principal
        );

        // Asignar mecánicos adicionales
        cout << "\n¿Desea asignar mecánicos adicionales? (s/n): ";
        string respuesta;
        getline(cin, respuesta);

        while (respuesta == "s" || respuesta == "S") {
            // Actualizar lista de mecánicos disponibles
            R = W.exec_params(
                "SELECT id_mecanico, nombre || ' ' || apellidos "
                "FROM Mecanicos "
                "WHERE disponible = true AND id_mecanico != $1 "
                "ORDER BY id_mecanico",
                mecanico_principal
            );

            if (R.empty()) {
                cout << "No hay más mecánicos disponibles" << endl;
                break;
            }

            mecanicos.clear();
            for (const auto &row : R) {
                mecanicos.emplace_back(
                    row[0].as<int>(),
                    row[1].as<string>()
                );
            }

            // Mostrar mecánicos disponibles
            cout << "\nMecánicos disponibles:" << endl;
            cout << string(40, '-') << endl;
            for (const auto &mec : mecanicos) {
                cout << mec.first << ". " << mec.second << endl;
            }
            cout << "0. Cancelar" << endl;
            cout << string(40, '-') << endl;

            int mecanico_adicional = obtenerNumero("Seleccione un mecánico adicional: ", 0, 999999);
            if (mecanico_adicional == 0) break;

            // Verificar que el mecánico seleccionado existe
            encontrado = false;
            for (const auto &mec : mecanicos) {
                if (mec.first == mecanico_adicional) {
                    encontrado = true;
                    break;
                }
            }
            if (!encontrado) {
                cout << "Mecánico no válido" << endl;
                continue;
            }

            // Registrar mecánico adicional
            try {
                W.exec_params(
                    "INSERT INTO Mecanicos_Reparacion (id_reparacion, id_mecanico) "
                    "VALUES ($1, $2)",
                    id_reparacion, mecanico_adicional
                );

                W.exec_params(
                    "UPDATE Mecanicos SET disponible = false WHERE id_mecanico = $1",
                    mecanico_adicional
                );

                cout << "Mecánico adicional asignado exitosamente" << endl;
            } catch (const exception &e) {
                cout << "Error al asignar mecánico: " << e.what() << endl;
                continue;
            }

            cout << "¿Desea asignar otro mecánico? (s/n): ";
            getline(cin, respuesta);
        }

        W.commit();
        cout << "\nReparación registrada exitosamente" << endl;

    } catch (const exception &e) {
        cout << "\nError: " << e.what() << endl;
    }
    esperarEntrada();
}

void generarFactura(connection &c) {
    limpiarPantalla();
    cout << "\n=== GENERACIÓN DE FACTURA ===\n" << endl;

    try {
        // Obtener y mostrar vehículos con reparaciones
        vector<pair<string, string>> vehiculos;
        {
            work W(c);
            result R = W.exec(
                "SELECT DISTINCT v.matricula, v.modelo || ' (' || v.matricula || ') - ' || "
                "c.nombre || ' ' || c.apellidos "
                "FROM Vehiculos v "
                "JOIN Clientes c ON v.dni_cliente = c.dni "
                "JOIN Reparaciones r ON v.matricula = r.matricula_vehiculo "
                "WHERE r.estado = 'Finalizada' "
                "ORDER BY v.matricula"
            );

            for (const auto &row : R) {
                vehiculos.emplace_back(
                    row[0].as<string>(), // matricula
                    row[1].as<string>()  // descripción
                );
            }
            W.commit();
        }

        if (vehiculos.empty()) {
            cout << "No hay reparaciones finalizadas" << endl;
            esperarEntrada();
            return;
        }

        // Mostrar vehículos
        cout << "\nVehículos disponibles:" << endl;
        cout << string(40, '-') << endl;
        for (size_t i = 0; i < vehiculos.size(); ++i) {
            cout << (i + 1) << ". " << vehiculos[i].second << endl;
        }
        cout << "0. Cancelar" << endl;
        cout << string(40, '-') << endl;

        int seleccion = obtenerNumero("Seleccione una opción: ", 0, (int)vehiculos.size());
        if (seleccion == 0) return;

        string matricula = vehiculos[seleccion - 1].first;

        work W(c);
        // Obtener datos de la reparación
        result R = W.exec_params(
            "SELECT r.id_reparacion, r.fecha_entrada, r.mano_obra, "
            "c.dni, c.nombre, c.apellidos, c.direccion, "
            "v.modelo, v.color, "
            "m.nombre as mec_nombre, m.apellidos as mec_apellidos "
            "FROM Reparaciones r "
            "JOIN Vehiculos v ON r.matricula_vehiculo = v.matricula "
            "JOIN Clientes c ON v.dni_cliente = c.dni "
            "JOIN Mecanicos m ON r.mecanico_principal = m.id_mecanico "
            "WHERE v.matricula = $1 AND r.estado = 'Finalizada' "
            "ORDER BY r.fecha_entrada DESC LIMIT 1",
            matricula
        );

        if (R.empty()) {
            cout << "No se encontró la reparación" << endl;
            esperarEntrada();
            return;
        }

        auto row = R[0];
        int id_reparacion = row["id_reparacion"].as<int>();
        double mano_obra = row["mano_obra"].as<double>();

        // Imprimir factura
        cout << "\n=====================================\n";
        cout << "          FACTURA REPARACIÓN         \n";
        cout << "=====================================\n\n";
        
        cout << "CLIENTE\n";
        cout << "DNI: " << row["dni"].as<string>() << "\n";
        cout << "Nombre: " << row["nombre"].as<string>() << " " 
             << row["apellidos"].as<string>() << "\n";
        cout << "Dirección: " << row["direccion"].as<string>() << "\n\n";

        cout << "VEHÍCULO\n";
        cout << "Matrícula: " << matricula << "\n";
        cout << "Modelo: " << row["modelo"].as<string>() << "\n";
        cout << "Color: " << row["color"].as<string>() << "\n\n";

        cout << "MECÁNICO PRINCIPAL\n";
        cout << row["mec_nombre"].as<string>() << " " 
             << row["mec_apellidos"].as<string>() << "\n\n";

        // Obtener y mostrar repuestos
        cout << "REPUESTOS UTILIZADOS\n";
        cout << string(40, '-') << "\n";
        cout << setw(20) << left << "Repuesto" 
             << setw(10) << right << "Cantidad"
             << setw(10) << "Precio" << "\n";
        cout << string(40, '-') << "\n";

        double total_repuestos = 0;
        R = W.exec_params(
            "SELECT r.nombre, r.precio_unidad, rr.cantidad "
            "FROM Repuestos_Reparacion rr "
            "JOIN Repuestos r ON rr.id_repuesto = r.id_repuesto "
            "WHERE rr.id_reparacion = $1",
            id_reparacion
        );

        for (const auto &rep : R) {
            double subtotal = rep["precio_unidad"].as<double>() * rep["cantidad"].as<int>();
            total_repuestos += subtotal;
            cout << setw(20) << left << rep["nombre"].as<string>()
                 << setw(10) << right << rep["cantidad"].as<int>()
                 << setw(10) << fixed << setprecision(2) << subtotal << "\n";
        }

        // Calcular totales
        double subtotal = total_repuestos + mano_obra;
        double iva = subtotal * 0.16;
        double total_euros = subtotal + iva;
        long total_pesetas = total_euros * 166.386;

        cout << "\nRESUMEN\n";
        cout << string(40, '-') << "\n";
        cout << setw(30) << left << "Mano de obra:" 
             << setw(10) << right << mano_obra << "€\n";
        cout << setw(30) << left << "Total repuestos:" 
             << setw(10) << right << total_repuestos << "€\n";
        cout << setw(30) << left << "Subtotal:" 
             << setw(10) << right << subtotal << "€\n";
        cout << setw(30) << left << "IVA (16%):" 
             << setw(10) << right << iva << "€\n";
        cout << string(40, '-') << "\n";
        cout << setw(30) << left << "TOTAL (EUROS):" 
             << setw(10) << right << total_euros << "€\n";
        cout << setw(30) << left << "TOTAL (PESETAS):" 
             << setw(10) << right << total_pesetas << " pts.\n";
        cout << string(40, '-') << "\n";

    } catch (const exception &e) {
        cout << "\nError: " << e.what() << endl;
    }
    esperarEntrada();
}

void consultarReparacionesCliente(connection &c) {
    limpiarPantalla();
    cout << "\n=== CONSULTA DE REPARACIONES POR CLIENTE ===\n" << endl;

    try {
        // Obtener y mostrar lista de clientes
        vector<pair<string, string>> clientes;  // <dni, descripción>
        {
            work W(c);
            result R = W.exec(
                "SELECT dni, nombre || ' ' || apellidos || ' (DNI: ' || dni || ')' "
                "FROM Clientes ORDER BY apellidos, nombre"
            );

            for (const auto &row : R) {
                clientes.emplace_back(
                    row[0].as<string>(),  // DNI
                    row[1].as<string>()   // Nombre completo + DNI
                );
            }
            W.commit();
        }

        if (clientes.empty()) {
            cout << "No hay clientes registrados" << endl;
            esperarEntrada();
            return;
        }

        // Mostrar clientes
        cout << "\nClientes disponibles:" << endl;
        cout << string(40, '-') << endl;
        for (size_t i = 0; i < clientes.size(); ++i) {
            cout << (i + 1) << ". " << clientes[i].second << endl;
        }
        cout << "0. Cancelar" << endl;
        cout << string(40, '-') << endl;

        int seleccion = obtenerNumero("Seleccione una opción: ", 0, (int)clientes.size());
        if (seleccion == 0) return;

        string dni = clientes[seleccion - 1].first;  // Obtenemos el DNI real

        work W(c);
        result R = W.exec_params(
            "SELECT c.nombre, c.apellidos, "
            "v.matricula, v.modelo, "
            "r.id_reparacion, r.fecha_entrada, r.fecha_fin, r.estado, r.mano_obra, "
            "m.nombre as mec_nombre, m.apellidos as mec_apellidos "
            "FROM Clientes c "
            "JOIN Vehiculos v ON c.dni = v.dni_cliente "
            "JOIN Reparaciones r ON v.matricula = r.matricula_vehiculo "
            "JOIN Mecanicos m ON r.mecanico_principal = m.id_mecanico "
            "WHERE c.dni = $1 "
            "ORDER BY r.fecha_entrada DESC",
            dni
        );

        if (R.empty()) {
            cout << "\nNo hay reparaciones registradas para este cliente" << endl;
            esperarEntrada();
            return;
        }

        cout << "\nHistorial de reparaciones para " 
             << R[0]["nombre"].as<string>() << " " 
             << R[0]["apellidos"].as<string>() << "\n\n";

        for (const auto &row : R) {
            cout << string(50, '=') << "\n";
            cout << "Reparación #" << row["id_reparacion"].as<int>() << "\n";
            cout << "Vehículo: " << row["modelo"].as<string>() 
                 << " (" << row["matricula"].as<string>() << ")\n";
            cout << "Fecha entrada: " << row["fecha_entrada"].as<string>() << "\n";
            
            if (!row["fecha_fin"].is_null()) {
                cout << "Fecha finalización: " << row["fecha_fin"].as<string>() << "\n";
            }
            
            cout << "Estado: " << row["estado"].as<string>() << "\n";
            cout << "Mecánico principal: " << row["mec_nombre"].as<string>() 
                 << " " << row["mec_apellidos"].as<string>() << "\n";
            
            // Mostrar mecánicos adicionales
            result R2 = W.exec_params(
                "SELECT m.nombre, m.apellidos "
                "FROM Mecanicos_Reparacion mr "
                "JOIN Mecanicos m ON mr.id_mecanico = m.id_mecanico "
                "WHERE mr.id_reparacion = $1",
                row["id_reparacion"].as<int>()
            );

            if (!R2.empty()) {
                cout << "Mecánicos adicionales:\n";
                for (const auto &mec : R2) {
                    cout << "  - " << mec["nombre"].as<string>() 
                         << " " << mec["apellidos"].as<string>() << "\n";
                }
            }
            cout << string(50, '-') << "\n";
        }

    } catch (const exception &e) {
        cout << "\nError: " << e.what() << endl;
    }
    esperarEntrada();
}

void consultarEstadoMecanicos(connection &c) {
    limpiarPantalla();
    cout << "\n=== CONSULTA DE ESTADO DE MECÁNICOS ===\n" << endl;

    try {
        work W(c);
        result R = W.exec(
            "SELECT m.id_mecanico, m.nombre, m.apellidos, m.disponible, "
            "COUNT(r.id_reparacion) as reparaciones_activas "
            "FROM Mecanicos m "
            "LEFT JOIN Reparaciones r ON m.id_mecanico = r.mecanico_principal "
            "AND r.estado != 'Finalizada' "
            "GROUP BY m.id_mecanico "
            "ORDER BY m.id_mecanico"
        );

        if (R.empty()) {
            cout << "No hay mecánicos registrados" << endl;
            esperarEntrada();
            return;
        }

        cout << "\nEstado actual de los mecánicos:\n";
        cout << string(70, '=') << "\n";
        
        for (const auto &row : R) {
            cout << row["nombre"].as<string>() << " " 
                 << row["apellidos"].as<string>() << "\n";
            cout << "Estado: " << (row["disponible"].as<bool>() ? 
                                 "Disponible" : "Ocupado") << "\n";
            cout << "Reparaciones activas: " 
                 << row["reparaciones_activas"].as<int>() << "\n";

            // Mostrar reparaciones actuales
            result R2 = W.exec_params(
                "SELECT v.modelo, v.matricula, r.fecha_entrada, r.estado "
                "FROM Reparaciones r "
                "JOIN Vehiculos v ON r.matricula_vehiculo = v.matricula "
                "WHERE r.mecanico_principal = $1 AND r.estado != 'Finalizada'",
                row["id_mecanico"].as<int>()
            );

            if (!R2.empty()) {
                cout << "Trabajos actuales:\n";
                for (const auto &rep : R2) {
                    cout << "  - " << rep["modelo"].as<string>() 
                         << " (" << rep["matricula"].as<string>() << ")"
                         << " - " << rep["estado"].as<string>() << "\n";
                }
            }
            cout << string(70, '-') << "\n";
        }

    } catch (const exception &e) {
        cout << "\nError: " << e.what() << endl;
    }
    esperarEntrada();
}

void menuPrincipal(connection &c) {
    int opcion;
    do {
        limpiarPantalla();
        cout << "\n=== SISTEMA DE GESTIÓN - TALLER MECÁNICO ===\n" << endl;
        cout << "1. Registros\n";
        cout << "2. Reparaciones\n";
        cout << "3. Consultas\n";
        cout << "0. Salir\n\n";
        
        opcion = obtenerNumero<int>("Seleccione una opción: ", 0, 3);

        switch (opcion) {
            case 1: {
                limpiarPantalla();
                cout << "\n=== MENÚ DE REGISTROS ===\n" << endl;
                cout << "1. Registrar cliente y vehículo\n";
                cout << "0. Volver\n\n";
                
                int subopcion = obtenerNumero<int>("Seleccione una opción: ", 0, 1);
                if (subopcion == 1) registrarClienteVehiculo(c);
                break;
            }
            case 2: {
                limpiarPantalla();
                cout << "\n=== MENÚ DE REPARACIONES ===\n" << endl;
                cout << "1. Asignar mecánicos a reparación\n";
                cout << "2. Generar factura\n";
                cout << "0. Volver\n\n";
                
                int subopcion = obtenerNumero<int>("Seleccione una opción: ", 0, 2);
                switch (subopcion) {
                    case 1: asignarMecanicos(c); break;
                    case 2: generarFactura(c); break;
                }
                break;
            }
            case 3: {
                limpiarPantalla();
                cout << "\n=== MENÚ DE CONSULTAS ===\n" << endl;
                cout << "1. Consultar reparaciones de cliente\n";
                cout << "2. Consultar estado de mecánicos\n";
                cout << "0. Volver\n\n";
                
                int subopcion = obtenerNumero<int>("Seleccione una opción: ", 0, 2);
                switch (subopcion) {
                    case 1: consultarReparacionesCliente(c); break;
                    case 2: consultarEstadoMecanicos(c); break;
                }
                break;
            }
        }
    } while (opcion != 0);
}

int main() {
    try {
        limpiarPantalla();
        cout << "\n=== TALLER MECÁNICO - SISTEMA DE GESTIÓN ===\n" << endl;
        
        cout << "Conectando a la base de datos..." << endl;
        connection c = conectarBD();
        
        cout << "Verificando estructura de datos..." << endl;
        crearTablas(c);
        
        cout << "Sistema iniciado correctamente" << endl;
        esperarEntrada();
        
        menuPrincipal(c);
        
        return 0;
    } catch (const exception &e) {
        cerr << "\nError fatal: " << e.what() << endl;
        esperarEntrada();
        return 1;
    }
}