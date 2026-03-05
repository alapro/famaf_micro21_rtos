# Guía de Supervivencia: Lenguaje C para Sistemas Embebidos y RTOS

**Asignatura:** Sistemas de Tiempo Real  
**Profesor:** Agustín Miguel Laprovitta  
**Institución:** FAMAF - UNC  
**Carrera:** Licenciatura en Ciencias de la Computación  
**Conocimientos Previos Requeridos:** Organización del Computador, Sistemas Operativos  

---

En el desarrollo de aplicaciones de escritorio, el sistema operativo y el compilador abstraen la complejidad del hardware y gestionan la memoria de forma casi ilimitada. En la programación *bare-metal* o bajo un RTOS, el lenguaje C deja de ser una abstracción y se convierte en una herramienta para manipular compuertas lógicas y direccionar memorias físicas con extrema precisión.

Esta guía resume los 12 recursos críticos y "trampas" del estándar C que son vitales para la supervivencia en la programación de sistemas embebidos de tiempo real.

## 1. El Modificador `volatile`: El antídoto contra el optimizador
Los compiladores modernos (como GCC con banderas `-O2` o `-O3`) son agresivos optimizando lecturas redundantes. Si una variable es modificada por un agente externo al flujo secuencial del código (como una interrupción de hardware o un periférico), el compilador no lo notará y "cacheará" el valor en un registro de la CPU, ignorando los cambios reales en la RAM.

* **Regla de Oro:** Toda variable global compartida entre una ISR (Rutina de Servicio de Interrupción) y el flujo principal, y todo puntero a un registro de hardware, **debe** declararse como `volatile`.
* **Qué hace:** Fuerza a la CPU a ejecutar una instrucción de lectura/escritura en la memoria física en *cada* acceso a la variable.

```c
// INCORRECTO: El compilador optimizará el while en un loop infinito.
int flag_evento = 0; 

// CORRECTO: La CPU leerá la memoria en cada iteración.
volatile int flag_evento = 0; 

void ISR_Hardware() {
    flag_evento = 1; // Modificado asíncronamente
}
```

## 2. El Modificador `static`: Privacidad y salvación del Stack
En un entorno RTOS, cada tarea posee su propio bloque de memoria reservado para la Pila (*Stack*), el cual suele ser muy pequeño (ej. 256 bytes). El uso estratégico de `static` previene la corrupción de memoria.

* **Protección del Stack:** Al declarar una variable grande dentro de una función como `static`, su almacenamiento se traslada de la pila a la sección `.bss` o `.data` (RAM global). 
* **Encapsulamiento:** Declarar variables globales o funciones como `static` a nivel de archivo (`.c`) las oculta del *linker*. Esto previene que otro módulo acceda a ellas accidentalmente usando `extern`.

```c
static QueueHandle_t xColaSensores; // Privado a este archivo

void Task_Procesamiento(void *pvParameters) {
    static uint8_t buffer_gigante[1024]; // No consume el Stack de la tarea
    for(;;) { /* Lógica */ }
}
```
## 3. El Modificador `const`: Seguridad, Memoria y la Danza de los Punteros
En aplicaciones de escritorio, `const` se utiliza principalmente para prevenir errores lógicos. En sistemas embebidos, tiene un impacto arquitectónico directo: **decide en qué memoria física vivirá el dato** y define contratos estrictos de manipulación de memoria.

* **El Ahorro de RAM:** Las variables globales/estáticas regulares van a la RAM. Si se declaran como `const` (ej. tablas de búsqueda o *strings* de error), el *linker* las ubica en la sección `.rodata` (Read-Only Data), leyéndose directamente desde la memoria Flash/ROM y salvando preciados kilobytes de RAM.

### 3.1. Disquisición de Punteros: ¿Dónde va el asterisco?
Cuando combinamos `const` con punteros, la posición del asterisco (`*`) lo cambia absolutamente todo. La regla de oro en C para leer estas declaraciones es **leer de derecha a izquierda**.



**Caso A: Puntero a una constante (`const uint8_t * ptr`)**
* **Lectura:** `ptr` es un puntero a un `uint8_t` que es `const`.
* **Comportamiento:** El puntero es libre de moverse y apuntar a otras direcciones, pero **no puedes modificar el dato** al que apunta a través de él. Es de solo lectura.
* **Caso de Uso en RTOS (Contratos de Función):** Al pasar buffers a una Cola del RTOS o a un driver de transmisión, garantizamos que la función no corromperá los datos originales.
~~~c
// El driver de UART promete no alterar tu buffer
void UART_Transmit(const uint8_t * buffer, uint16_t size) {
    // buffer[0] = 0xFF; // ERROR DE COMPILACIÓN: El dato es de solo lectura
    buffer++;            // PERMITIDO: El puntero puede avanzar al siguiente byte
}
~~~

**Caso B: Puntero constante (`uint8_t * const ptr`)**
* **Lectura:** `ptr` es un `const` puntero a un `uint8_t`.
* **Comportamiento:** La dirección de memoria que guarda el puntero está anclada de por vida. Sin embargo, **el dato al que apunta sí puede ser modificado**.
* **Caso de Uso en Embebidos (Mapeo de Hardware):** Es la forma matemáticamente correcta de definir el puntero a un periférico físico. El puerto GPIOA siempre estará en la misma dirección física del silicio, pero sus registros internos cambian constantemente.
~~~c
// El puntero siempre apuntará a 0x40020000, pero podemos escribir en esa dirección
GPIO_TypeDef * const GPIOA_PTR = (GPIO_TypeDef *) 0x40020000;

GPIOA_PTR->ODR = 0xFF; // PERMITIDO: Modificamos el dato (los registros físicos)
// GPIOA_PTR = NULL;   // ERROR DE COMPILACIÓN: No puedes mover el puntero
~~~

**Caso C: Puntero constante a una constante (`const uint8_t * const ptr`)**
* **Lectura:** `ptr` es un `const` puntero a un `uint8_t` que es `const`.
* **Comportamiento:** Bloqueo total. Ni el puntero puede apuntar a otro lado, ni los datos pueden modificarse.
* **Caso de Uso en Embebidos (Look-up Tables en ROM):** Vectores matemáticos precalculados o fuentes para un display LCD que se graban de forma inmutable en la memoria Flash.
~~~c
const uint8_t * const TABLA_SENOS = (const uint8_t *) 0x08004000;
~~~

## 4. Tipos de Datos Deterministas: La librería `<stdint.h>`
El estándar original de C define que un `int` representa el tamaño natural de la palabra del procesador (16 bits en un AVR, 32 bits en un ARM Cortex-M). Esta incertidumbre es inaceptable en sistemas críticos.

* **La Solución:** Todo código embebido debe incluir `<stdint.h>` y utilizar tipos de ancho fijo.
* **Regla Estricta:** Eliminar el uso de tipos nativos desnudos (`int`, `long`, `char`). Utilizar exclusivamente `uint8_t`, `int16_t`, `uint32_t`, etc., para garantizar portabilidad y alineación en memoria.

## 5. Promoción de Tipos y Conversión Automática (El Peligro Oculto)
El lenguaje C fue diseñado para ser rápido, por lo que intenta adaptar el tamaño de las variables al tamaño natural de los registros de la ALU (Unidad Aritmético Lógica) del procesador subyacente. Para lograrlo, realiza conversiones implícitas silenciosas que no generan *warnings* al compilar, pero que causan estragos en sistemas embebidos.



### 5.1. Promoción Entera (Integer Promotion)
Cualquier tipo de dato más pequeño que un `int` (como `int8_t`, `uint8_t` o `int16_t`) es automáticamente "promovido" (convertido) a un `int` de tamaño completo antes de realizar cualquier operación aritmética, lógica o a nivel de bit. 

* **El problema arquitectónico:** En un microcontrolador de 8 bits (ej. AVR), el `int` tiene 16 bits. En un ARM Cortex-M, el `int` tiene 32 bits. Un código que asume cómo se desbordará (*overflow*) una variable al sumarla, se comportará de manera radicalmente distinta si cambias de microcontrolador.
* **El peligro en los *Shifts*:** Si tomas un `uint8_t` que vale `1` y lo desplazas 15 veces a la izquierda (`1 << 15`), en lugar de desbordarse y quedar en `0` (como esperaría un alumno al ver que la variable es de 8 bits), el compilador lo promueve a un `int` de 32 bits, realiza el desplazamiento, y el resultado es válido pero sorpresivo.

### 5.2. Extensión de Signo (Sign Extension)
Cuando el compilador promueve una variable *con signo* a un tamaño mayor, copia el Bit Más Significativo (MSB, que indica el signo) para llenar los nuevos bits agregados. Esto mantiene el valor matemático, pero altera completamente la máscara de bits esperada.

~~~c
int8_t valor_negativo = 0x80; // Representa -128 en complemento a 2
int32_t promovido = valor_negativo; 

// Los alumnos suelen esperar que 'promovido' sea 0x00000080
// Realidad: La extensión de signo lo convierte en 0xFFFFFF80 (-128)
~~~

Si ese valor extendido se utiliza luego para aplicar una máscara a un registro de hardware, terminará sobreescribiendo configuraciones en los 24 bits superiores.

### 5.3. Conversiones Aritméticas Habituales (La Trampa del Signo)
Si en una expresión (comparación aritmética o asignación) se mezclan variables de distinto tipo, C intenta igualarlas. La regla letal: si se mezcla una variable con signo (signed) y una sin signo (unsigned) del mismo rango, la variable con signo es forzada a convertirse en unsigned.

~~~C
int32_t temperatura_actual = -5; // Hace frío
uint32_t umbral_alarma = 100;    

if (temperatura_actual > umbral_alarma) {
    // ¡ESTE BLOQUE SE EJECUTARÁ! 
    // -5 es convertido a unsigned, transformándose en 0xFFFFFFFB (4.294.967.291)
    // Matemáticamente para la CPU: 4294967291 > 100
    DispararAlarmaIncendio(); 
}
~~~
### 5.4. Reglas de Supervivencia Defensiva
Para evitar que el compilador tome decisiones por nosotros, debemos aplicar rigor:

* Jamás mezclar signos: Nunca comparar ni operar matemáticamente un int32_t con un uint32_t en la misma expresión.

* Casting Explícito: Si la mezcla es inevitable, usar un cast explícito para forzar al compilador a comparar bajo nuestros términos, demostrando la intención del diseño: if ( temperatura_actual > (int32_t)umbral_alarma ).

* Sufijos Literales: Por defecto, cualquier número escrito en el código (ej. 250 o 0x80) es tratado como un int (entero con signo). Al definir máscaras de bits o constantes de hardware, se debe obligar al compilador a tratarlas como números sin signo usando los sufijos U o UL (Unsigned Long).

~~~C
#define MASCARA_CRITICA 0x80000000U // El sufijo 'U' previene la extensión de signo
~~~

## 6. El Estándar MISRA C: Programación Defensiva en Sistemas Críticos
En Ciencias de la Computación, el objetivo en las primeras materias suele ser que el algoritmo sea eficiente y resuelva el problema. En sistemas embebidos de misión crítica (donde un fallo cuesta vidas o millones de dólares), eso no alcanza: el código debe ser **matemáticamente verificable** y estar libre de comportamientos indefinidos.

### 6.1. ¿Qué es y de dónde viene?
MISRA (Motor Industry Software Reliability Association) publicó este estándar por primera vez en 1998 en el Reino Unido. Nació por una necesidad urgente: los fabricantes de automóviles empezaron a llenar los vehículos con ECUs (Unidades de Control Electrónico) programadas en C, un lenguaje potentísimo pero lleno de "zonas grises" (comportamientos que el estándar de C deja a merced de quien diseñó el compilador). 

Hoy en día, MISRA C trascendió por completo la industria automotriz. Es un estándar *de facto* y, a menudo, un requisito legal para certificar software aeroespacial (DO-178C), equipamiento médico (IEC 62304), ferroviario y de defensa.

### 6.2. La Filosofía
El lenguaje C asume que el programador sabe exactamente lo que hace y nunca se equivoca. MISRA asume que los humanos cometen errores. El estándar no te dice *cómo* programar tu lógica, sino que **restringe el uso del lenguaje a un subconjunto seguro**, prohibiendo prácticas que son difíciles de analizar estáticamente o propensas a errores. 

Su cumplimiento no se verifica "a ojo" en un *Code Review*, sino obligando a que el código pase por herramientas de Análisis Estático de Código (como PC-lint, Cppcheck o Polyspace) antes de que el compilador siquiera lo toque.



### 6.3. Reglas Clásicas (El choque con el C de escritorio)
Para un estudiante, enfrentarse a MISRA es frustrante porque prohíbe prácticas que son comunes en el software de PC. Estas son algunas de las reglas fundamentales que impactan directamente al diseñar un RTOS:

* **Prohibición de Memoria Dinámica (Dir. 4.12 / Regla 21.3):** No se permite usar `malloc`, `calloc`, `realloc` ni `free`.
  * *El motivo:* La fragmentación de la memoria a largo plazo es matemáticamente inevitable. Además, el tiempo que tarda el sistema operativo en buscar un bloque libre en el *Heap* no es determinista, destruyendo el cálculo del Tiempo de Ejecución en el Peor Caso (WCET). En un RTOS, los *stacks* de las tareas y las colas deben preasignarse estáticamente.
* **Prohibición de Recursividad (Regla 17.2):** Las funciones no pueden llamarse a sí mismas, ni directa ni indirectamente.
  * *El motivo:* Impide que las herramientas de análisis calculen el uso máximo de la pila (*Worst-Case Stack Usage*). En un microcontrolador con *stacks* diminutos de 256 bytes por tarea, una recursividad no acotada es una garantía absoluta de un *Stack Overflow*. Hay que reemplazarla por diseño iterativo (`while` / `for`).
* **Cero Asignaciones en Expresiones Booleanas (Regla 13.4):** Prohíbe evaluar una condición y asignar una variable al mismo tiempo.
  * *El motivo:* Previene el error de tipeo más frecuente y letal de la historia de C (usar `=` en lugar de `==`).
  ~~~c
  // PROHIBIDO POR MISRA (y altamente peligroso)
  if (estado = leer_sensor()) { /* ... */ }

  // CÓDIGO CONFORME A MISRA:
  estado = leer_sensor();
  if (estado != 0U) { /* ... */ }
  ~~~
* **Un Único Punto de Salida por Función (Regla 15.5):** Una función debería tener un solo `return` al final de la misma (aunque estándares modernos lo han flexibilizado un poco para salidas tempranas por error, la regla general prevalece).
  * *El motivo:* Múltiples `return` diseminados por el código ("código espagueti") hacen que el flujo de ejecución sea difícil de seguir y facilitan que el programador olvide liberar un recurso crítico (como un Mutex de FreeRTOS o apagar un periférico) antes de salir de la función.
* **Tipos de Datos Básicos Estrictos (Dir. 4.6):** Como vimos en la sección de `<stdint.h>`, MISRA prohíbe el uso de tipos básicos nativos (`int`, `char`, `long`). Exige explícitamente el uso de tipos que indiquen tamaño y signo (ej. `int32_t`, `uint16_t`) para evitar que el código cambie de comportamiento al compilarse para una arquitectura distinta.

## 7. Aritmética de Punto Fijo: Evitando el Punto Flotante
Muchos microcontroladores no poseen Unidad de Punto Flotante (FPU) por hardware. Usar `float` inyecta pesadas librerías matemáticas por software, destruyendo el tiempo de ejecución (WCET). 

Incluso con FPU de hardware, usar flotantes en una tarea obliga al RTOS a guardar y restaurar decenas de registros extra en cada *Context Switch*, aumentando drásticamente la latencia.

* **La Solución:** Utilizar enteros asumiendo un factor de escala (ej. almacenar milivoltios en lugar de voltios) o usar el formato fraccionario Q (ej. Q15.16), donde las operaciones toman 1 solo ciclo de reloj de la CPU.

## 8. Mapeo de Hardware: `struct` y Punteros Físicos
Los periféricos se controlan escribiendo en registros mapeados en memoria. En lugar de usar punteros crudos, se modelan los bloques físicos mediante estructuras.

```c
typedef struct {
    volatile uint32_t MODER;   // Offset 0x00
    volatile uint32_t OTYPER;  // Offset 0x04
    // ... otros registros (abreviado para legibilidad)
    volatile uint32_t ODR;     // Offset 0x14: Salida
} GPIO_TypeDef;

#define GPIOA ((GPIO_TypeDef *) 0x40020000) // Dirección física base

// Uso directo sobre el hardware
GPIOA->ODR = 0xFFFF; 
```
## 9. Operadores de Bits: Máscaras, Shifts y el Patrón Read-Modify-Write
Los registros de hardware empaquetan múltiples configuraciones independientes en una sola palabra de 32 bits. Escribir directamente en el registro (ej. `GPIOA->MODER = 0x01;`) es destructivo, ya que sobrescribe la configuración de todos los demás pines que comparten ese registro. 

Para manipular el hardware de forma segura, se deben usar operaciones lógicas a nivel de bit (Bitwise) y el sufijo `U` (Unsigned) para evitar la extensión de signo inducida por el compilador.


### Operaciones Básicas (Un solo bit)
Para operar sobre un bit específico (ej. bit 5), primero creamos una máscara desplazando un `1` sin signo (`1U`) a esa posición:

~~~c
#define BIT_5 (1U << 5)  // Resultado en binario: 0010 0000
~~~

* **SET (Poner a 1):** Usamos el operador lógico OR (`|`).
  ~~~c
  GPIOA->ODR |= BIT_5;
  ~~~
* **CLEAR (Poner a 0):** Usamos AND (`&`) combinado con NOT (`~`). Crea una máscara invertida (`1101 1111`).
  ~~~c
  GPIOA->ODR &= ~BIT_5;
  ~~~
* **TOGGLE (Invertir estado):** Usamos XOR (`^`).
  ~~~c
  GPIOA->ODR ^= BIT_5;
  ~~~
* **READ (Leer estado):** Usamos AND (`&`) para enmascarar los demás bits y verificar.
  ~~~c
  if ((GPIOA->IDR & BIT_5) != 0) {
      // El bit 5 está en ALTO (1)
  }
  ~~~

### Operaciones Avanzadas: Patrón RMW (Read-Modify-Write)
A menudo, un campo ocupa varios bits. Por ejemplo, si los bits 4 y 5 controlan la velocidad de un pin y queremos cargar el valor `2` (binario `10`), no podemos usar simplemente OR (`|`) porque si el registro tenía un `1` previo, el resultado será `3`.

Se debe usar el patrón RMW en dos pasos estrictos:

~~~c
// 1. Definimos la máscara para el campo (2 bits en 1 = 0b11 o 3U)
#define MASCARA_VELOCIDAD (3U << 4)

// 2. Definimos el nuevo valor que queremos cargar
#define VELOCIDAD_ALTA (2U << 4)

// PASO A: CLEAR (Limpiamos el campo actual dejándolo en 00)
GPIOA->OSPEEDR &= ~MASCARA_VELOCIDAD;

// PASO B: SET (Escribimos el nuevo valor partiendo de 00)
GPIOA->OSPEEDR |= VELOCIDAD_ALTA;
~~~

## 10. Punteros a Función (Callbacks)
Vitales para sistemas dirigidos por eventos. Permiten registrar funciones que el sistema operativo o un driver llamará asíncronamente. En FreeRTOS, toda tarea se pasa al Kernel como un puntero a función.

```c
typedef void (*TaskFunction_t)( void * );

void MiTarea(void *parametros) {
    while(1) { /* Lógica */ }
}

// Se pasa el puntero de la función al Scheduler
xTaskCreate(MiTarea, "MainTask", 128, NULL, 1, NULL);
```
## 11. Directivas del Preprocesador: Controlando la Compilación a Costo Cero
El preprocesador de C ejecuta una pasada de reemplazo de texto antes de que comience la compilación real. En sistemas embebidos y RTOS, estas directivas (`#`) son la herramienta principal para abstraer el hardware y gestionar configuraciones sin consumir un solo ciclo de CPU ni un byte de memoria ROM en tiempo de ejecución.



### 11.1. Guardas de Inclusión (Include Guards)
En C, si un archivo `.h` se incluye múltiples veces en distintos módulos, el compilador arrojará un error de "redefinición". Las guardas previenen esto asegurando que el contenido del encabezado se procese una única vez por unidad de compilación.

~~~c
// archivo: hardware_config.h
#ifndef HARDWARE_CONFIG_H  // Si no está definido...
#define HARDWARE_CONFIG_H  // ...lo defino ahora.

// Contenido del archivo (estructuras, prototipos, etc.)
void inicializar_reloj(void);

#endif // HARDWARE_CONFIG_H
~~~
*(Nota: Aunque muchos compiladores modernos soportan `#pragma once` con el mismo propósito, `#ifndef` es el estándar universal y garantiza portabilidad absoluta entre distintos toolchains).*

### 11.2. Compilación Condicional (`#ifdef`, `#if`)
Permite incluir o excluir bloques enteros de código según banderas definidas al momento de compilar. Es vital para dos escenarios típicos en el laboratorio:

**A. Selección de Hardware (Portabilidad):**
Podemos usar el mismo archivo `.c` para distintas placas. El código que no cumple la condición literalmente "desaparece" antes de compilarse, ahorrando memoria Flash.
~~~c
// Esta macro suele pasarse desde el Makefile o el IDE (ej. flag -D PLACA_STM32)
#define PLACA_STM32  

#if defined(PLACA_STM32)
    #include "stm32f4xx.h"
    #define PIN_LED GPIO_Pin_5
#elif defined(PLACA_ESP32)
    #include "esp32_hal.h"
    #define PIN_LED 2
#else
    #error "Debe definir una placa de hardware válida para compilar."
#endif
~~~

**B. Trazabilidad y Debugging:**
Imprimir mensajes por puerto serie (`printf`) consume muchísimo tiempo y arruina el comportamiento de tiempo real. Podemos dejar los mensajes en el código, pero hacer que solo se compilen en modo *Debug*.
~~~c
#define MODO_DEBUG 1

#if MODO_DEBUG == 1
    // Si estamos depurando, DEBUG_PRINT se reemplaza por printf
    #define DEBUG_PRINT(x) printf(x)
#else
    // En producción, DEBUG_PRINT desaparece por completo (costo cero)
    #define DEBUG_PRINT(x) 
#endif

void Tarea_Critica() {
    DEBUG_PRINT("Iniciando tarea...\n"); // En producción, esta línea no se compila.
}
~~~

### 11.3. El Peligro de las Macros con Argumentos
Es común usar `#define` para crear funciones "inline" rápidas. Sin embargo, como el preprocesador hace un reemplazo de texto "tonto" sin entender la lógica de C, puede generar *bugs* sutiles por evaluación múltiple.

~~~c
#define MAXIMO(a, b) ((a) > (b) ? (a) : (b))

void calcular() {
    int x = 5;
    int y = 8;
    // El preprocesador lo expande ciegamente a:
    // int resultado = ((x++) > (y++) ? (x++) : (y++));
    int resultado = MAXIMO(x++, y++); 
    
    // ERROR FATAL: La variable 'y' se incrementará DOS veces en la misma línea.
}
~~~

* **Regla de Supervivencia:** Usar funciones declaradas como `static inline` en lugar de macros complejas siempre que sea posible. El compilador las optimizará igual de bien (insertando el código directamente sin overhead de llamada a función), pero aplicando validación de tipos estricta y evitando la evaluación múltiple de los argumentos.

## 12. Modificadores de Función: `inline` y `weak`
En el desarrollo de software tradicional, el costo de invocar a una función (guardar el contexto actual en el stack, hacer un salto en la memoria, y luego retornar) es insignificante. En sistemas de tiempo real con plazos de microsegundos, ese *overhead* puede hacer que se pierda un *deadline*. Además, las librerías de hardware embebido requieren mecanismos flexibles para que el usuario pueda inyectar su propio código sin modificar los archivos originales del fabricante.

### 12.1. El modificador `inline`: Velocidad a cambio de Memoria
La palabra clave `inline` es una sugerencia al compilador para que, en lugar de realizar un salto a la subrutina, copie y pegue literalmente el cuerpo de la función en cada lugar donde es invocada.



* **Ventaja (Velocidad):** Elimina el *overhead* de la llamada a la función (no hay que apilar registros ni hacer *branching*). Es ideal para funciones matemáticas cortas o manipulaciones de bits críticas dentro de una interrupción.
* **Desventaja (Consumo de ROM):** Si la función es muy larga y se llama desde 20 lugares distintos, el tamaño del binario final (huella en la memoria Flash) crecerá drásticamente.
* **La Práctica Segura (`static inline`):** Es la alternativa robusta a las peligrosas macros del preprocesador (vistas en el punto 11.3). Al definirlas como `static inline` en un archivo `.h`, garantizamos validación estricta de tipos sin generar errores del *linker* por redefinición.

~~~c
// En un archivo utilidades.h
// El compilador reemplazará la llamada por la instrucción de hardware directa
static inline void resetear_watchdog(void) {
    IWDG->KR = 0xAAAA; // Instrucción atómica
}
~~~

### 12.2. El atributo `weak`: Plantillas y Sobreescritura Limpia
Aunque no es estrictamente parte del estándar C original (es una extensión del compilador, típicamente `__attribute__((weak))` en GCC), es imposible programar en ARM Cortex-M sin entenderlo. 

El modificador `weak` (débil) le dice al *linker*: *"Aquí tienes una implementación por defecto de esta función. Úsala, a menos que el usuario escriba en otro lado una función exactamente con el mismo nombre. Si lo hace, ignora esta y usa la del usuario"*.



* **Uso Típico 1 (Interrupciones por defecto):** Los archivos de arranque (*startup code*) definen todos los *Handlers* de interrupción como `weak` apuntando a un bucle infinito. Si el alumno no necesita usar el puerto serie, no hace nada. Si lo necesita, simplemente escribe `void USART1_IRQHandler(void)` en su `main.c` y el *linker* hace el reemplazo automáticamente.
* **Uso Típico 2 (Callbacks de la HAL):** Las librerías de abstracción de hardware proveen funciones `weak` vacías para que el usuario inserte su lógica cuando ocurre un evento, sin tener que tocar el código fuente de la librería.

~~~c
// --- DENTRO DE LA LIBRERÍA DEL FABRICANTE (No tocar) ---
__attribute__((weak)) void HAL_UART_RxCpltCallback(void) {
    // Implementación por defecto: No hace absolutamente nada.
    // Previene que el programa colapse si el usuario olvidó crearla.
}


// --- DENTRO DEL CÓDIGO DEL ALUMNO (main.c) ---
// Como tiene el mismo nombre, el linker descarta la función 'weak'
// y enlaza esta implementación fuerte ('strong').
void HAL_UART_RxCpltCallback(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Despertamos a la tarea del RTOS que procesa el dato
    vTaskNotifyGiveFromISR(xTaskUART, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
~~~


