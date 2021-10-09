logotipos-terminales
Bienvenido al repositorio de Windows Terminal, Console y Command-Line
Este repositorio contiene el código fuente de:

Terminal de Windows
Vista previa de la terminal de Windows
El host de la consola de Windows ( conhost.exe)
Componentes compartidos entre los dos proyectos
ColorTool
Proyectos de muestra que muestran cómo consumir las API de la consola de Windows
Los repositorios relacionados incluyen:

Documentación de la terminal de Windows ( Repo: Contribuir a los documentos )
Documentación de la API de la consola
Fuente Cascadia Code
Instalación y ejecución de Windows Terminal
🔴 Nota: Windows Terminal requiere Windows 10 1903 (compilación 18362) o posterior

Microsoft Store [recomendado]
Instale la Terminal de Windows desde Microsoft Store . Esto le permite estar siempre en la última versión cuando lanzamos nuevas compilaciones con actualizaciones automáticas.

Este es nuestro método preferido.

Otros métodos de instalación
Vía GitHub
Para los usuarios que no pueden instalar Windows Terminal desde Microsoft Store, las versiones publicadas se pueden descargar manualmente desde la página de versiones de este repositorio .

Descarga el Microsoft.WindowsTerminal_<versionNumber>.msixbundlearchivo de la sección Activos . Para instalar la aplicación, simplemente haga doble clic en el .msixbundlearchivo y el instalador de la aplicación debería ejecutarse automáticamente. Si eso falla por algún motivo, puede probar el siguiente comando en un indicador de PowerShell:

# NOTA: Si está usando PowerShell 7+, ejecute 
# Import-Module Appx -UseWindowsPowerShell 
# antes de usar Add-AppxPackage.

Complemento AppxPackage Microsoft.WindowsTerminal_ < versionNumber > .msixbundle
🔴 Nota: Si instala Terminal manualmente:

La terminal no se actualizará automáticamente cuando se publiquen nuevas compilaciones, por lo que deberá instalar regularmente la última versión de Terminal para recibir las últimas correcciones y mejoras.
A través de la CLI del Administrador de paquetes de Windows (también conocido como winget)
Los usuarios de winget pueden descargar e instalar la última versión de Terminal instalando el Microsoft.WindowsTerminal paquete:

winget install - id = Microsoft.WindowsTerminal - e
Vía Chocolatey (no oficial)
Los usuarios de Chocolatey pueden descargar e instalar la última versión de Terminal instalando el microsoft-windows-terminalpaquete:

choco instalar microsoft - windows - terminal
Para actualizar Windows Terminal usando Chocolatey, ejecute lo siguiente:

actualización de choco microsoft - windows - terminal
Si tiene algún problema al instalar / actualizar el paquete, vaya a la página del paquete de Windows Terminal y siga el proceso de clasificación de Chocolatey

A través de Scoop (no oficial)
Los usuarios de Scoop pueden descargar e instalar la última versión de Terminal instalando el windows-terminalpaquete:

cuchara cuchara añadir extras
primicia instalar windows - terminal
Para actualizar Windows Terminal usando Scoop, ejecute lo siguiente:

actualización de primicia de windows - terminal
Si tiene algún problema al instalar / actualizar el paquete, busque o informe el mismo en la página de problemas del repositorio de cubos de Scoop Extras.

Hoja de ruta de Windows Terminal 2.0
El plan para entregar Windows Terminal 2.0 se describe aquí y se actualizará a medida que avanza el proyecto.

Estado de construcción del proyecto
Proyecto	Estado de la construcción
Terminal	Estado de construcción de la terminal
ColorTool	Estado de construcción de Colortool
Descripción general de la terminal y la consola
Tómese unos minutos para revisar la descripción general a continuación antes de sumergirse en el código:

Terminal de Windows
Windows Terminal es una aplicación de terminal productiva, nueva, moderna y rica en funciones para usuarios de línea de comandos. Incluye muchas de las funciones solicitadas con más frecuencia por la comunidad de línea de comandos de Windows, incluida la compatibilidad con pestañas, texto enriquecido, globalización, configurabilidad, temas y estilos, y más.

La Terminal también deberá cumplir con nuestros objetivos y medidas para garantizar que siga siendo rápida y eficiente, y que no consuma grandes cantidades de memoria o energía.

El host de la consola de Windows
El host de la consola de Windows conhost.exe, es la experiencia de usuario de la línea de comandos original de Windows. También aloja la infraestructura de la línea de comandos de Windows y el servidor API de la consola de Windows, el motor de entrada, el motor de renderizado, las preferencias del usuario, etc. El código de host de la consola en este repositorio es la fuente real a partir de la cual conhost.exese construye en Windows.

Desde que asumió la propiedad de la línea de comandos de Windows en 2014, el equipo agregó varias características nuevas a la consola, incluida la transparencia de fondo, la selección basada en líneas, soporte para secuencias de terminal virtual / ANSI , color de 24 bits , una pseudoconsola ("ConPTY" ) y más.

Sin embargo, debido a que el objetivo principal de la consola de Windows es mantener la compatibilidad con versiones anteriores, no hemos podido agregar muchas de las características que la comunidad (y el equipo) han estado esperando durante los últimos años, incluidas pestañas, texto Unicode y emoji.

Estas limitaciones nos llevaron a crear la nueva Terminal de Windows.

Puede leer más sobre la evolución de la línea de comandos en general, y la línea de comandos de Windows específicamente en esta serie adjunta de publicaciones de blog en el blog del equipo de Línea de comandos.

Componentes compartidos
Mientras revisábamos la Consola de Windows, modernizamos considerablemente su base de código, separando claramente las entidades lógicas en módulos y clases, introdujimos algunos puntos clave de extensibilidad, reemplazamos varias colecciones y contenedores antiguos y locales por contenedores STL más seguros y eficientes , y simplificamos y simplificamos el código. más seguro mediante el uso de las bibliotecas de implementación de Windows de Microsoft - WIL .

Esta revisión dio como resultado que varios de los componentes clave de la consola estuvieran disponibles para su reutilización en cualquier implementación de terminal en Windows. Estos componentes incluyen un nuevo motor de renderizado y diseño de texto basado en DirectWrite, un búfer de texto capaz de almacenar tanto UTF-16 como UTF-8, un analizador / emisor VT y más.

Creando la nueva Terminal de Windows
Cuando comenzamos a planificar la nueva aplicación de Terminal de Windows, exploramos y evaluamos varios enfoques y pilas de tecnología. Finalmente, decidimos que nuestros objetivos se cumplirían mejor si continuamos con nuestra inversión en nuestro código base C ++, lo que nos permitiría reutilizar varios de los componentes modernizados antes mencionados tanto en la Consola existente como en la nueva Terminal. Además, nos dimos cuenta de que esto nos permitiría construir gran parte del núcleo de la Terminal como un control de interfaz de usuario reutilizable que otros pueden incorporar en sus propias aplicaciones.

El resultado de este trabajo está contenido en este repositorio y se entrega como la aplicación de Windows Terminal que puede descargar desde Microsoft Store o directamente desde las versiones de este repositorio .

Recursos
Para obtener más información sobre Windows Terminal, algunos de estos recursos pueden resultarle útiles e interesantes:

Blog de línea de comandos
Serie de blogs de antecedentes de la línea de comandos
Lanzamiento de la terminal de Windows: Terminal "Sizzle Video"
Lanzamiento de Windows Terminal: Build 2019 Session
Ejecutar como radio: Show 645 - Terminal de Windows con Richard Turner
Podcast de Azure Devops: Episodio 54 - Kayla Cinnamon y Rich Turner en DevOps en la terminal de Windows
Sesión de Microsoft Ignite 2019: La moderna línea de comandos de Windows: Terminal de Windows - BRK3321
Preguntas más frecuentes
Construí y ejecuté la nueva Terminal, pero se parece a la consola anterior
Causa: está iniciando la solución incorrecta en Visual Studio.

Solución: asegúrese de que está compilando e implementando el CascadiaPackageproyecto en Visual Studio.

⚠Nota: OpenConsole.exees solo una conhost.execonsola clásica de Windows construida localmente que aloja la infraestructura de línea de comandos de Windows. Windows Terminal utiliza OpenConsole para conectarse y comunicarse con aplicaciones de línea de comandos (a través de ConPty ).

Documentación
Toda la documentación del proyecto se encuentra en aka.ms/terminal-docs . Si desea contribuir a la documentación, envíe una solicitud de extracción en el repositorio de documentación de la terminal de Windows .

Contribuyendo
¡Estamos emocionados de trabajar junto a ustedes, nuestra increíble comunidad, para construir y mejorar Windows Terminal!

ANTES de comenzar a trabajar en una función / corrección , lea y siga nuestra Guía del colaborador para evitar cualquier esfuerzo en vano o duplicado.

Comunicarse con el equipo
La forma más sencilla de comunicarse con el equipo es a través de problemas de GitHub.

Presente nuevos problemas, solicitudes de funciones y sugerencias, pero DEBE buscar problemas preexistentes abiertos / cerrados similares antes de crear un nuevo problema.

Si desea hacer una pregunta que cree que no justifica un problema (todavía), comuníquese con nosotros a través de Twitter:

Kayla Cinnamon, directora del programa: @cinnamon_msft
Dustin Howett, jefe de ingeniería: @dhowett
Michael Niksa, desarrollador sénior: @michaelniksa
Mike Griese, desarrollador: @zadjii
Carlos Zamora, Desarrollador: @cazamor_msft
Leon Liang, Desarrollador: @leonmsft
Pankaj Bhojwani, desarrollador
Leonard Hecker, Desarrollador: @LeonardHecker
Orientación para desarrolladores
Prerrequisitos
Debe ejecutar Windows 1903 (compilación> = 10.0.18362.0) o posterior para ejecutar Windows Terminal
Debe habilitar el modo de desarrollador en la aplicación de configuración de Windows para instalar y ejecutar localmente Windows Terminal
Debe tener PowerShell 7 o posterior instalado
Debe tener instalado el SDK de Windows 10 1903
Debe tener al menos VS 2019 instalado
Debe instalar las siguientes cargas de trabajo mediante VS Installer. Nota: Al abrir la solución en VS 2019, se le pedirá que instale los componentes que faltan automáticamente :
Desarrollo de escritorio con C ++
Desarrollo de plataforma universal de Windows
Los siguientes componentes individuales
Herramientas de la plataforma universal de Windows C ++ (v142)
Construyendo el Código
Este repositorio usa submódulos git para algunas de sus dependencias. Para asegurarse de que los submódulos se restauren o actualicen, asegúrese de ejecutar lo siguiente antes de compilar:

actualización del submódulo git --init --recursive
OpenConsole.sln se puede construir desde Visual Studio o desde la línea de comandos usando un conjunto de herramientas y scripts convenientes en el directorio / tools :

Construyendo en PowerShell
Import-Module . \ Tools \ OpenConsole.psm1
 Set-MsBuildDevEnvironment 
Invoke-OpenConsoleBuild
Edificio en Cmd
. \ t ools \ r azzle.cmd
bcz
Ejecución y depuración
Para depurar la Terminal de Windows en VS, haga clic derecho en CascadiaPackage(en el Explorador de soluciones) y vaya a Propiedades. En el menú Depurar, cambie "Proceso de aplicación" y "Proceso de tarea en segundo plano" a "Solo nativo".

A continuación, debería poder crear y depurar el proyecto de Terminal pulsando F5.

👉Usted no será capaz de lanzar el terminal directamente mediante la ejecución del WindowsTerminal.exe. Para obtener más detalles sobre el motivo, consulte el n . ° 926 , n . ° 4043

Orientación de codificación
Revise estos breves documentos a continuación sobre nuestras prácticas de codificación.

👉 Si encuentra que falta algo en estos documentos, no dude en contribuir a cualquiera de nuestros archivos de documentación en cualquier lugar del repositorio (¡o escriba algunos nuevos!)

Este es un trabajo en progreso a medida que aprendemos lo que necesitaremos proporcionar a las personas para que sean contribuyentes efectivos a nuestro proyecto.

Estilo de codificación
Organización del código
Excepciones en nuestra base de código heredada
Punteros inteligentes y macros útiles para interactuar con Windows en WIL
Código de conducta
Este proyecto ha adoptado el Código de conducta de código abierto de Microsoft . Para obtener más información, consulte las preguntas frecuentes sobre el Código de conducta o póngase en contacto con opencode@microsoft.com si tiene preguntas o comentarios adicionales.
