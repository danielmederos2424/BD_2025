#include <iostream>
#include <pqxx/pqxx>
#include <cstdlib>
#include <limits>
#include <string>
#include <algorithm>
#include <regex>
#include <vector> 

using namespace std;
using namespace pqxx;

// Funciones auxiliares
void limpiarPantalla() { system("clear"); }

void limpiarBuffer() {
   cin.clear();
   cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void pausa() {
   cout << "\nPresione Enter para continuar...";
   cin.get();
}

void ejecutarConsulta(connection &c, const string &query) {
   try {
       work W(c);
       W.exec(query);
       W.commit();
   } catch(const exception &e) {
       cerr << "\033[1;31mError SQL: " << e.what() << "\033[0m\n";
       throw;
   }
}

connection conectar() {
   try {
       connection c("dbname=inmobiliaria user=postgres password=1234admin hostaddr=127.0.0.1 port=5432");
       if(!c.is_open()) throw runtime_error("No se pudo establecer conexión");
       return c;
   } catch(const exception &e) {
       cerr << "\033[1;31mError de conexión: " << e.what() << "\033[0m\n";
       throw;
   }
}

void crearTablasInmobiliaria(connection &c) {
   try {
       // Tabla Oficinas
       ejecutarConsulta(c, "CREATE TABLE IF NOT EXISTS Oficinas ("
                          "ID SERIAL PRIMARY KEY,"
                          "Nombre VARCHAR(100),"
                          "Direccion VARCHAR(200),"
                          "Telefono VARCHAR(20));");

       // Tabla Inmuebles actualizada
       ejecutarConsulta(c, "CREATE TABLE IF NOT EXISTS Inmuebles ("
                          "Ref SERIAL PRIMARY KEY,"
                          "Tipo VARCHAR(50),"
                          "Superficie INTEGER CHECK (Superficie > 0),"
                          "Direccion VARCHAR(100),"
                          "Zona VARCHAR(50),"
                          "PrecioVenta FLOAT,"
                          "PrecioAlquiler FLOAT,"
                          "Propietario VARCHAR(100),"
                          "TelefonoPropietario VARCHAR(20),"
                          "OficinaID INTEGER REFERENCES Oficinas(ID),"
                          "TieneLlaves BOOLEAN DEFAULT false);");

       // Tabla CaracteristicasVivienda
       ejecutarConsulta(c, "CREATE TABLE IF NOT EXISTS CaracteristicasVivienda ("
                          "Ref INTEGER PRIMARY KEY REFERENCES Inmuebles(Ref) ON DELETE CASCADE,"
                          "NumHabitaciones INTEGER,"
                          "NumBanos INTEGER,"
                          "NumAseos INTEGER,"
                          "NumCocinas INTEGER,"
                          "Altura INTEGER,"
                          "TieneGasCiudad BOOLEAN,"
                          "TienePuertaBlindada BOOLEAN,"
                          "TieneParquet BOOLEAN,"
                          "TieneCalefaccion BOOLEAN);");

       // Tabla CaracteristicasLocal
       ejecutarConsulta(c, "CREATE TABLE IF NOT EXISTS CaracteristicasLocal ("
                          "Ref INTEGER PRIMARY KEY REFERENCES Inmuebles(Ref) ON DELETE CASCADE,"
                          "Diafano BOOLEAN,"
                          "NumPuertas INTEGER,"
                          "TieneAireAcondicionado BOOLEAN,"
                          "SuperficieAltillo FLOAT);");

       // Tabla Visitas
       ejecutarConsulta(c, "CREATE TABLE IF NOT EXISTS Visitas ("
                          "ID SERIAL PRIMARY KEY,"
                          "RefInmueble INTEGER REFERENCES Inmuebles(Ref) ON DELETE CASCADE,"
                          "FechaHora TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                          "Cliente VARCHAR(100),"
                          "Comentario TEXT);");

   } catch(const exception &e) {
       cerr << "\033[1;31mError al crear tablas: " << e.what() << "\033[0m\n";
       throw;
   }
}

// Validaciones
template<typename T>
T validarEntradaNumerica(const string& mensaje, T minimo = 0, T maximo = numeric_limits<T>::max()) {
    T valor;
    while (true) {
        cout << mensaje;
        if (cin >> valor) {
            if (valor >= minimo && valor <= maximo) {
                limpiarBuffer();
                return valor;
            }
            cout << "\033[1;31mValor fuera de rango. Debe estar entre " 
                 << minimo << " y " << maximo << ".\033[0m\n";
        } else {
            cout << "\033[1;31mEntrada inválida. Introduzca un número válido.\033[0m\n";
            limpiarBuffer();
        }
    }
}

string validarEntradaTexto(const string& mensaje, int minLongitud = 1, int maxLongitud = 100) {
    string texto;
    while (true) {
        cout << mensaje;
        getline(cin, texto);
        
        // Eliminar espacios al inicio y final
        texto.erase(0, texto.find_first_not_of(" \t"));
        texto.erase(texto.find_last_not_of(" \t") + 1);
        
        if (!texto.empty() && texto.length() >= minLongitud && texto.length() <= maxLongitud) {
            return texto;
        }
        
        cout << "\033[1;31mEntrada inválida. La longitud debe estar entre " 
             << minLongitud << " y " << maxLongitud << " caracteres.\033[0m\n";
    }
}

bool validarEntradaBooleana(const string& mensaje) {
    char respuesta;
    while (true) {
        cout << mensaje << " (s/n): ";
        cin >> respuesta;
        limpiarBuffer();
        
        respuesta = tolower(respuesta);
        if (respuesta == 's') return true;
        if (respuesta == 'n') return false;
        
        cout << "\033[1;31mRespuesta inválida. Use 's' o 'n'.\033[0m\n";
    }
}

void agregarInmueble(connection &c) {
   limpiarPantalla();
   try {
       work W(c);
       
       // Validación de oficina
       result R = W.exec("SELECT ID, Nombre FROM Oficinas ORDER BY ID;");
       if (R.empty()) {
           cout << "\033[1;31mNo hay oficinas registradas. Registre una oficina primero.\033[0m\n";
           pausa();
           return;
       }

       cout << "Oficinas disponibles:\n";
       for(auto row : R) {
           cout << row[0].as<int>() << ". " << row[1].as<string>() << "\n";
       }
       
       // Validar selección de oficina
       int maxOficinaId = R[R.size() - 1][0].as<int>();
       int oficinaId = validarEntradaNumerica<int>("Seleccione oficina: ", 1, maxOficinaId);

       // Validar tipo de inmueble con lista predefinida
       vector<string> tiposValidos = {"Piso", "Casa", "Villa", "Local"};
       string tipo;
       while (true) {
           tipo = validarEntradaTexto("Tipo (Piso/Casa/Villa/Local): ", 3, 20);
           
           // Convertir a mayúsculas para comparación
           transform(tipo.begin(), tipo.end(), tipo.begin(), ::toupper);
           
           // Verificar si el tipo es válido
           auto it = find_if(tiposValidos.begin(), tiposValidos.end(), 
                             [&tipo](const string& t) { 
                                 return t == tipo; 
                             });
           
           if (it != tiposValidos.end()) break;
           
           cout << "\033[1;31mTipo de inmueble no válido. Elija entre Piso, Casa, Villa o Local.\033[0m\n";
       }
       
       // Validar superficie
       int superficie = validarEntradaNumerica<int>("Superficie (m2): ", 1, 50000);
       
       // Validar dirección con formato más estricto
       string direccion = validarEntradaTexto("Dirección (ej. Calle Mayor 10, 2º A): ", 5, 100);
       
       // Validar zona
       string zona = validarEntradaTexto("Zona: ", 3, 50);
       
       // Validar precios con rangos más realistas
       float precioVenta = validarEntradaNumerica<float>("Precio venta (0 si no está en venta): ", 0, 50000000.0);
       
       float precioAlquiler = validarEntradaNumerica<float>("Precio alquiler (0 si no está en alquiler): ", 0, 100000.0);
       
       // Validar nombre de propietario
       string propietario = validarEntradaTexto("Nombre del propietario: ", 3, 100);
       
       // Validar teléfono con formato específico
       string telefono;
       while (true) {
           telefono = validarEntradaTexto("Teléfono (formato: +34 612 345 678 o 612345678): ", 9, 20);
           
           // Validación básica de formato de teléfono
           regex telefonoRegex("(\\+34\\s?)?[6-9]\\d{8}");
           if (regex_match(telefono, telefonoRegex)) break;
           
           cout << "\033[1;31mFormato de teléfono inválido. Debe ser un número de 9 dígitos.\033[0m\n";
       }
       
       // Validar existencia de llaves
       bool tieneLlaves = validarEntradaBooleana("¿Tiene llaves?");

       // Insertar inmueble
       string query = "INSERT INTO Inmuebles (Tipo, Superficie, Direccion, Zona, "
                     "PrecioVenta, PrecioAlquiler, Propietario, TelefonoPropietario, "
                     "OficinaID, TieneLlaves) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10) "
                     "RETURNING Ref;";

       result res = W.exec_params(query, 
                                tipo, superficie, direccion, zona,
                                precioVenta, precioAlquiler, propietario, telefono,
                                oficinaId, tieneLlaves);

       int refInmueble = res[0][0].as<int>();

       // Características específicas según tipo
       if(tipo != "Local") {
           // Validaciones para viviendas
           int numHab = validarEntradaNumerica<int>("Número de habitaciones: ", 1, 20);
           int numBanos = validarEntradaNumerica<int>("Número de baños: ", 1, 10);
           int numAseos = validarEntradaNumerica<int>("Número de aseos: ", 0, 5);
           int numCocinas = validarEntradaNumerica<int>("Número de cocinas: ", 1, 3);
           
           bool gasCity = validarEntradaBooleana("¿Tiene gas ciudad?");
           bool puertaBlin = validarEntradaBooleana("¿Tiene puerta blindada?");
           bool parquet = validarEntradaBooleana("¿Tiene parquet?");
           bool calef = validarEntradaBooleana("¿Tiene calefacción?");

           W.exec_params("INSERT INTO CaracteristicasVivienda VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10)",
                        refInmueble, numHab, numBanos, numAseos, numCocinas, 
                        0, // Altura (no solicitada en la versión original)
                        gasCity, puertaBlin, parquet, calef);
       } else {
           // Validaciones para locales
           bool diafano = validarEntradaBooleana("¿Es diáfano?");
           int numPuertas = validarEntradaNumerica<int>("Número de puertas: ", 1, 10);
           bool aire = validarEntradaBooleana("¿Tiene aire acondicionado?");
           float supAltillo = validarEntradaNumerica<float>("Superficie del altillo (m2): ", 0, 1000.0);

           W.exec_params("INSERT INTO CaracteristicasLocal VALUES ($1,$2,$3,$4,$5)",
                        refInmueble, diafano, numPuertas, aire, supAltillo);
       }

       W.commit();
       cout << "\033[1;32mInmueble registrado exitosamente con referencia " << refInmueble << ".\033[0m\n";
       
   } catch(const exception &e) {
       cerr << "\033[1;31mError al registrar inmueble: " << e.what() << "\033[0m\n";
   }
   pausa();
}

void mostrarInmuebles(connection &c) {
   limpiarPantalla();
   try {
       nontransaction N(c);
       result R = N.exec(
           "SELECT i.*, o.Nombre as NombreOficina, "
           "CASE WHEN i.Tipo != 'Local' THEN "
           "    (SELECT json_build_object('habitaciones', cv.NumHabitaciones, "
           "                             'baños', cv.NumBanos, "
           "                             'aseos', cv.NumAseos, "
           "                             'cocinas', cv.NumCocinas, "
           "                             'gas_ciudad', cv.TieneGasCiudad, "
           "                             'puerta_blindada', cv.TienePuertaBlindada, "
           "                             'parquet', cv.TieneParquet) "
           "     FROM CaracteristicasVivienda cv WHERE cv.Ref = i.Ref) "
           "ELSE "
           "    (SELECT json_build_object('diafano', cl.Diafano, "
           "                             'puertas', cl.NumPuertas, "
           "                             'aire', cl.TieneAireAcondicionado, "
           "                             'altillo', cl.SuperficieAltillo) "
           "     FROM CaracteristicasLocal cl WHERE cl.Ref = i.Ref) "
           "END as caracteristicas "
           "FROM Inmuebles i "
           "LEFT JOIN Oficinas o ON i.OficinaID = o.ID "
           "ORDER BY i.Ref;"
       );

       for(result::const_iterator row = R.begin(); row != R.end(); ++row) {
           cout << "\033[1;36mReferencia: " << row["Ref"].as<int>() << "\033[0m\n";
           cout << "Tipo: " << row["Tipo"].as<string>() << "\n";
           cout << "Oficina: " << row["NombreOficina"].as<string>() << "\n";
           cout << "Superficie: " << row["Superficie"].as<int>() << "m2\n";
           cout << "Ubicación: " << row["Direccion"].as<string>() << " (" << row["Zona"].as<string>() << ")\n";
           if(!row["PrecioVenta"].is_null())
               cout << "Precio Venta: €" << row["PrecioVenta"].as<float>() << "\n";
           if(!row["PrecioAlquiler"].is_null())
               cout << "Precio Alquiler: €" << row["PrecioAlquiler"].as<float>() << "/mes\n";
           cout << "Propietario: " << row["Propietario"].as<string>() << "\n";
           cout << "Teléfono: " << row["TelefonoPropietario"].as<string>() << "\n";
           cout << "Llaves: " << (row["TieneLlaves"].as<bool>() ? "Sí" : "No") << "\n";
           cout << "Características: " << row["caracteristicas"].as<string>() << "\n";
           cout << "----------------------------------------\n";
       }
   } catch(const exception &e) {
       cerr << "\033[1;31mError al mostrar inmuebles: " << e.what() << "\033[0m\n";
   }
   pausa();
}

void eliminarInmueble(connection &c) {
   limpiarPantalla();
   try {
       work W(c);
       cout << "=== ELIMINAR INMUEBLE ===\n\n";

       result R = W.exec("SELECT i.Ref, i.Tipo, i.Direccion, o.Nombre "
                        "FROM Inmuebles i "
                        "LEFT JOIN Oficinas o ON i.OficinaID = o.ID "
                        "ORDER BY i.Ref;");

       if(R.empty()) {
           cout << "No hay inmuebles registrados\n";
           return;
       }

       cout << "Inmuebles disponibles:\n";
       for(auto row : R) {
           cout << "Ref " << row[0].as<int>() << ": " 
                << row[1].as<string>() << " - "
                << row[2].as<string>() << " ("
                << row[3].as<string>() << ")\n";
       }

       int ref;
       cout << "\nIngrese referencia a eliminar (0 para cancelar): ";
       cin >> ref;
       limpiarBuffer();

       if(ref == 0) {
           cout << "Operación cancelada\n";
           return;
       }

       cout << "\n¿Está seguro? (s/n): ";
       char confirm;
       cin >> confirm;
       limpiarBuffer();

       if(confirm != 's' && confirm != 'S') return;

       W.exec_params("DELETE FROM Inmuebles WHERE Ref = $1", ref);
       W.commit();
       
       cout << "\033[1;32mInmueble eliminado correctamente\033[0m\n";

   } catch(const exception &e) {
       cerr << "\033[1;31mError: " << e.what() << "\033[0m\n";
   }
   pausa();
}

void buscarInmuebles(connection &c) {
   limpiarPantalla();
   try {
       cout << "=== BÚSQUEDA DE INMUEBLES ===\n\n"
            << "1. Por zona\n"
            << "2. Por tipo\n"
            << "3. Por rango de precio\n"
            << "4. Por oficina\n"
            << "5. Por características\n"
            << "Opción: ";

       int opcion;
       cin >> opcion;
       limpiarBuffer();

       work W(c);
       string baseQuery = "SELECT i.*, o.Nombre as Oficina FROM Inmuebles i "
                         "LEFT JOIN Oficinas o ON i.OficinaID = o.ID WHERE ";
       string where;

       switch(opcion) {
           case 1: {
               cout << "Zona: ";
               string zona;
               getline(cin, zona);
               where = "LOWER(i.Zona) LIKE LOWER('%" + W.esc(zona) + "%')";
               break;
           }
           case 2: {
               cout << "Tipo: ";
               string tipo;
               getline(cin, tipo);
               where = "LOWER(i.Tipo) LIKE LOWER('%" + W.esc(tipo) + "%')";
               break;
           }
           case 3: {
               cout << "Precio mínimo: ";
               float min;
               cin >> min;
               cout << "Precio máximo: ";
               float max;
               cin >> max;
               where = "(i.PrecioVenta BETWEEN " + to_string(min) + " AND " + to_string(max) + 
                      " OR i.PrecioAlquiler BETWEEN " + to_string(min) + " AND " + to_string(max) + ")";
               break;
           }
           case 4: {
               result R = W.exec("SELECT ID, Nombre FROM Oficinas");
               cout << "\nOficinas:\n";
               for(auto row : R) {
                   cout << row[0].as<int>() << ". " << row[1].as<string>() << "\n";
               }
               cout << "Seleccione oficina: ";
               int id;
               cin >> id;
               where = "i.OficinaID = " + to_string(id);
               break;
           }
           case 5: {
               cout << "\nBuscar por:\n"
                    << "1. Número de habitaciones\n"
                    << "2. Baños\n"
                    << "3. Local diáfano\n"
                    << "Opción: ";
               int subOpcion;
               cin >> subOpcion;
               
               switch(subOpcion) {
                   case 1: {
                       cout << "Mínimo de habitaciones: ";
                       int hab;
                       cin >> hab;
                       where = "EXISTS (SELECT 1 FROM CaracteristicasVivienda cv "
                              "WHERE cv.Ref = i.Ref AND cv.NumHabitaciones >= " + to_string(hab) + ")";
                       break;
                   }
                   case 2: {
                       cout << "Mínimo de baños: ";
                       int ban;
                       cin >> ban;
                       where = "EXISTS (SELECT 1 FROM CaracteristicasVivienda cv "
                              "WHERE cv.Ref = i.Ref AND cv.NumBanos >= " + to_string(ban) + ")";
                       break;
                   }
                   case 3: {
                       where = "EXISTS (SELECT 1 FROM CaracteristicasLocal cl "
                              "WHERE cl.Ref = i.Ref AND cl.Diafano = true)";
                       break;
                   }
               }
               break;
           }
           default:
               cout << "Opción no válida\n";
               return;
       }

       result R = W.exec(baseQuery + where);
       cout << "\nResultados encontrados: " << R.size() << "\n\n";

       for(auto row : R) {
           cout << "Ref: " << row["Ref"].as<int>() << " | "
                << row["Tipo"].as<string>() << " | "
                << row["Superficie"].as<int>() << "m2 | "
                << row["Oficina"].as<string>() << "\n";
       }

   } catch(const exception &e) {
       cerr << "\033[1;31mError: " << e.what() << "\033[0m\n";
   }
   pausa();
}

void menu(connection &c) {
   int opcion;
   do {
       limpiarPantalla();
       cout << "\033[1;34m=== SISTEMA INMOBILIARIO ===\033[0m\n\n"
            << "1. Registrar inmueble\n"
            << "2. Mostrar inmuebles\n"
            << "3. Buscar inmuebles\n"
            << "4. Eliminar inmueble\n"
            << "0. Salir\n\n"
            << "Opción: ";

       if(!(cin >> opcion)) {
           cin.clear();
           cin.ignore(numeric_limits<streamsize>::max(), '\n');
           opcion = -1;
       }
       limpiarBuffer();

       try {
           switch(opcion) {
               case 1: agregarInmueble(c); break;
               case 2: mostrarInmuebles(c); break;
               case 3: buscarInmuebles(c); break;
               case 4: eliminarInmueble(c); break;
               case 0: 
                   limpiarPantalla();
                   cout << "Finalizando...\n";
                   system("sleep 1");
                   break;
               default:
                   cout << "\033[1;31mOpción no válida\033[0m\n";
                   pausa();
           }
       } catch(const exception &e) {
           cerr << "\033[1;31mError: " << e.what() << "\033[0m\n";
           pausa();
       }
   } while(opcion != 0);
}

int main() {
   limpiarPantalla();
   cout << "\033[1;34m=== SISTEMA INMOBILIARIO ===\033[0m\n\n";
   
   try {
       connection c = conectar();
       system("sleep 1");
       
       cout << "Inicializando...\n";
       crearTablasInmobiliaria(c);
       system("sleep 1");
       
       menu(c);
       
   } catch(const exception &e) {
       cerr << "\033[1;31mError fatal: " << e.what() << "\033[0m\n";
       pausa();
       return 1;
   }
   
   return 0;
}