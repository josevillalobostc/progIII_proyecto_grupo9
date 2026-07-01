# Programación III: Proyecto Final (2026-1)
Repositorio: https://github.com/josevillalobostc/progIII_proyecto_grupo9

## Integrantes
* Catherine Yennifer Lopez Chavez
* Keyra Amira Huamanyauri Alvarado
* José Luis Villalobos Jiménez


## Preprocesamiento de datos
Para tratar la data, se pasó todo a minúsculas, se quitó cualquier símbolo extraño, y se cambiaron letras de otros alfabetos como İ a una variante universal como i.

## Estructura de datos
Para el tratamiento de los nombres de películas, armaremos un suffix tree generalizado usando el algoritmo de Ukkonen, un algoritmo de complejidad lineal O(n) para una cadena de longitud n, que permite buscar cualquier subcadena m en un tiempo O(m). Cada hoja de este árbol (que corresponde a un sufijo del título de la película), tendrá almacenado el ID de la película a la cual pertenece, por lo que la busqueda de todas las películas que corresponden a cierta cadena se extendería a O(m + k), donde m es la longitud de la cadena, y k es la cantidad de hojas que salen del nodo al cual guía la cadena, que se buscarían mediante DFS simple (busqueda en profundidad para recorrer todas las posibles hojas).  

Para el caso de las sinopsis, al ser textos muy largos, usar el algoritmo de Ukkonen sería muy pesado, por lo que se podría implementar el uso de Inverted Index (O(1) apróximado) o Suffix Array (O(m log(n))) si se quiere tomar en cuenta subcadenas. Estos tienen una complejidad mayor en la busqueda pero la construcción de su estructura de datos (inserción) usa menos memoria, algo excelente para la cantidad de palabras de cada sinopsis.


## Instrucciones de Compilacion
Para poder ejecutar nuestro proyecto:
```bash
g++ -std=c++17 main.cpp -o programa
```
luego ejecutamos:
```bash
./programa
```
Y si estamos en windows
```powershell
programa.exe
```

El archivo compilara sin errores siempre y cuando la base de datos se encuentre en :
data/wiki_movie_plots_deduped.csv


## Pseudocodigo de construcción de suffix tree
```
ALGORITMO ConstruirSuffixTree(documento)

    Crear nodo raíz

    activeNode ← raíz
    activeEdge ← -1
    activeLength ← 0

    remainingSuffixCount ← 0

    leafEnd ← -1

    textoGlobal ← ""

    PARA cada documento EN documentos HACER

        // Agregar delimitador único
        documento ← documento + "#"

        Registrar posición inicial del documento

        Concatenar documento a textoGlobal

        // Construcción incremental del árbol
        PARA cada carácter del documento HACER

            leafEnd ← posición actual

            // Nuevo sufijo pendiente
            remainingSuffixCount ← remainingSuffixCount + 1

            lastNewNode ← NULL

            MIENTRAS remainingSuffixCount > 0 HACER

                // Si no existe longitud activa,
                // la arista activa será el carácter actual
                SI activeLength = 0 ENTONCES
                    activeEdge ← posición actual
                FIN SI

                // Buscar transición desde el nodo activo
                SI no existe hijo con textoGlobal[activeEdge] ENTONCES

                    // REGLA 2:
                    // Crear nueva hoja
                    Crear nodo hoja

                    hoja.start ← posición actual
                    hoja.end ← leafEnd

                    Conectar hoja a activeNode

                    // Conectar suffix link pendiente
                    SI lastNewNode ≠ NULL ENTONCES
                        lastNewNode.suffixLink ← activeNode
                        lastNewNode ← NULL
                    FIN SI

                SINO

                    next ← hijo correspondiente

                    // WALK DOWN
                    // Descender si activeLength supera
                    // la longitud de la arista
                    SI activeLength ≥ longitudArista(next) ENTONCES

                        activeEdge ← activeEdge + longitudArista(next)

                        activeLength ← activeLength - longitudArista(next)

                        activeNode ← next

                        CONTINUAR

                    FIN SI

                    // REGLA 3:
                    // El carácter ya existe
                    SI textoGlobal[next.start + activeLength]
                       = textoGlobal[posición actual] ENTONCES

                        SI lastNewNode ≠ NULL
                           Y activeNode ≠ raíz ENTONCES

                            lastNewNode.suffixLink ← activeNode

                            lastNewNode ← NULL

                        FIN SI

                        activeLength ← activeLength + 1

                        // No se necesita más inserción
                        ROMPER

                    FIN SI

                    // REGLA 2:
                    // División de arista

                    Crear nodo interno split

                    split.start ← next.start
                    split.end ← next.start + activeLength - 1

                    Reemplazar conexión original
                    entre activeNode y next

                    Crear nueva hoja para el carácter actual

                    Ajustar inicio de next

                    Conectar:
                        split → nueva hoja
                        split → next

                    // Manejo de suffix links
                    SI lastNewNode ≠ NULL ENTONCES
                        lastNewNode.suffixLink ← split
                    FIN SI

                    lastNewNode ← split

                FIN SI

                // Sufijo procesado
                remainingSuffixCount ← remainingSuffixCount - 1

                // Actualizar punto activo
                SI activeNode = raíz
                   Y activeLength > 0 ENTONCES

                    activeLength ← activeLength - 1

                    activeEdge ← siguiente sufijo pendiente

                SINO SI activeNode ≠ raíz ENTONCES

                    activeNode ← activeNode.suffixLink

                FIN SI

            FIN MIENTRAS

        FIN PARA

    FIN PARA

    // Construcción final de índices
    Recorrer árbol con DFS

    Asignar suffixIndex a cada hoja

FIN ALGORITMO
```

## Pseudocodigo de búsqueda
```
currNode ← raíz
i ← 0 // índice para recorrer la subcadena

MIENTRAS i < longitud(subcadena) HACER
    
    // buscar si existe una arista que empiece con el carácter actual
    SI no existe hijo de currNode que empiece con subcadena[i] ENTONCES
        RETORNAR lista vacía // La subcadena no existe
    FIN SI

    edge ← hijo correspondiente
    j ← edge.start
    
    // comparar el patrón con los caracteres de la arista
    MIENTRAS i < longitud(subcadena) Y j ≤ edge.end HACER
        SI subcadena[i] ≠ textoGlobal[j] ENTONCES
            RETORNAR lista vacía // mismatch en la arista
        FIN SI
        
        i ← i + 1
        j ← j + 1
    FIN MIENTRAS

    // si aún queda parte de la subcadena, pasamos al siguiente nodo
    SI i < longitud(subcadena) ENTONCES
        currNode ← edge
    FIN SI

FIN MIENTRAS


resultados ← lista vacía


SI i = longitud(subcadena) ENTONCES
    RecolectarIDs(edge, resultados)
FIN SI

RETORNAR resultados
```
Algoritmo de RecolectarIDs:
```
SI nodo es hoja ENTONCES
    resultados.agregar(nodo.movieID)
SINO
    // si es nodo interno, seguimos bajando por todos sus hijos
    PARA cada hijo DE nodo HACER
        RecolectarIDs(hijo, resultados)
    FIN PARA
FIN SI
```
## Frontend, Decorator y Observer

La interfaz del proyecto se implementó mediante una consola interactiva. Permitiendo buscar películas, mostrar resultados paginados de cinco en cinco, seleccionar una película, visualizar su sinopsis y ejecutar acciones como Like (me gusta) o Ver más tarde.

### Patrón Decorator

Se aplicó el patrón Decorator para construir la visualización de una película por capas. La clase `MovieRenderer` define la interfaz base. `BasicMovieRenderer` muestra los datos principales de la película. Luego, `SynopsisDecorator` agrega la sinopsis y `ActionButtonsDecorator` agrega las opciones interactivas.

Esto permite extender la forma de mostrar una película sin modificar la estructura `Movie` ni duplicar código.

### Patrón Observer

Se aplicó el patrón Observer para conectar las acciones del usuario con el gestor de películas del usuario. La clase `ConsoleUI` emite eventos cuando el usuario selecciona Like o Ver más tarde. La clase `UserMovieManager`, registrada como observadora, recibe esos eventos y actualiza las listas correspondientes.

De esta forma, la interfaz queda desacoplada de la lógica de gestión de usuario.

### Paginación

Los resultados de búsqueda se muestran en grupos de cinco películas. El usuario puede avanzar a los siguientes cinco resultados o seleccionar una película del grupo actual para ver su detalle.

### Recomendación basada en contenido

Se implementó un algoritmo Content-Based. Cada película se representa mediante un perfil de palabras obtenido de su título, género, director, casting y sinopsis. Cuando el usuario da Like a una película, el sistema compara ese perfil con el de las demás películas usando similitud de Jaccard. Las películas con mayor similitud se muestran como recomendaciones.