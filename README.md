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