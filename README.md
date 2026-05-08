# Programación III: Proyecto Final (2026-1)

## Integrantes
* Catherine Yennifer Lopez Chavez
* Keyra Amira Huamanyauri Alvarado
* José Luis Villalobos Jiménez


## Preprocesamiento de datos
Para tratar la data, se pasó todo a minúsculas, se quitó cualquier símbolo extraño, y se cambiaron letras de otros alfabetos como İ a una variante universal como i.

## Estructura de datos
Para el tratamiento de los nombres de películas, armaremos un suffix tree generalizado usando el algoritmo de Ukkonen, un algoritmo de complejidad lineal O(n) para una cadena de longitud n, que permite buscar cualquier subcadena m en un tiempo O(m). Cada hoja de este árbol (que corresponde a un sufijo del título de la película), tendrá almacenado el ID de la película a la cual pertenece, por lo que la busqueda de todas las películas que corresponden a cierta cadena se extendería a O(m + k), donde m es la longitud de la cadena, y k es la cantidad de hojas que salen del nodo al cual guía la cadena, que se buscarían mediante DFS simple (busqueda en profundidad para recorrer todas las posibles hojas).  

Para el caso de las sinopsis, al ser textos muy largos, usar el algoritmo de Ukkonen sería muy pesado, por lo que se podría implementar el uso de Inverted Index (O(1) apróximado) o Suffix Array (O(m log(n))) si se quiere tomar en cuenta subcadenas. Estos tienen una complejidad mayor en la busqueda pero la construcción de su estructura de datos (inserción) usa menos memoria, algo excelente para la cantidad de palabras de cada sinopsis.

## Pseudocodigo
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