# Medieval Tavern - 3D Graphics Project

**RAF Computer Graphics 2024/2025**  
**Student:** Vuk Urosevic  
**Index:** 64/22RN  
**Theme:** Interactive 3D Medieval Tavern with Advanced Lighting

### Project Overview
This project implements an immersive medieval tavern scene using OpenGL 3.3+ with the RAFGL framework. The focus is on realistic lighting, authentic tavern atmosphere, and advanced graphics techniques.

### Advanced Techniques Implemented

**Deferred Shading Pipeline**  
Complete G-buffer implementation with separate geometry and lighting passes. Uses high-precision textures (RGB16F) for position and normal data, enabling efficient multiple light rendering with complex lighting calculations.

**Screen-Space Ambient Occlusion (SSAO)**  
Implements circular sampling technique for realistic ambient occlusion. Enhances depth perception and provides realistic contact shadows in corners and crevices.

**Omnidirectional Point Light Shadows**  
Cube map shadow mapping provides 360-degree shadow casting from point lights. Includes dynamic shadow casting from interactive flashlight with high-resolution, artifact-free shadows.

**Post-Processing Pipeline**  
Flexible post-processing system with sepia tone effect for authentic medieval atmosphere. Toggle-able with SHIFT key to switch between normal colors and warm brown/sepia tones that enhance the tavern's historical ambiance.

### Scene Features

**Authentic Medieval Layout**  
Bar table with beer mugs, three dining tables with octagonal stools, professional .obj model integration. High-resolution floor geometry (50x50 subdivisions) provides detailed shadow receiving.

**Interactive Lighting System**  
Camera-mounted flashlight with F key toggle and scroll wheel distance control. Global light radius adjustment with Q/E keys. Dynamic candle flame animation with realistic flickering behavior.

**Object Hierarchy and Animation**  
Parent-child relationships for table candles with animated flame offsets. Procedural geometry generation for custom tavern elements including candle bases and architectural details.

### Controls
- **WASD** - Camera movement
- **Mouse** - Look around
- **F** - Toggle flashlight
- **Scroll Wheel** - Adjust flashlight distance
- **Q/E** - Modify global light radius  
- **R** - Reset flashlight distance
- **TAB** - Toggle shadow mode (all lights vs flashlight only)
- **SHIFT** - Toggle post-processing effect (sepia/medieval atmosphere)

### Build and Run
```bash
make clean
make build
make run
```

### Technical Implementation
Built with OpenGL 3.3+ and GLSL 3.30+ using the RAFGL framework. Features custom shader programs, procedural geometry generation, object hierarchies with programmatic animation, and multiple texture types beyond basic albedo mapping.

---

## Srpska Verzija

**RAF Računarska Grafika 2024/2025**  
**Student:** Vuk Urosevic  
**Index:** 64/22RN  
**Tema:** Interaktivna 3D Srednjovekovna Taverna sa Naprednim Osvetljenjem

### Pregled Projekta
Ovaj projekat implementira impresivnu scenu srednjovekovne taverne koristeći OpenGL 3.3+ sa RAFGL framework-om. Fokus je na realističnom osvetljenju, autentičnoj atmosferi taverne i naprednim grafičkim tehnikama.

### Napredne Tehnike

**Deferred Shading Pipeline**  
Kompletna G-buffer implementacija sa odvojenim geometry i lighting pass-ovima. Koristi visoko-precizne teksture (RGB16F) za poziciju i normale, omogućavajući efikasno renderovanje više svetala sa kompleksnim lighting kalkulacijama.

**Screen-Space Ambient Occlusion (SSAO)**  
Implementira circular sampling tehniku za realističku ambijentalnu okluziju. Poboljšava percepciju dubine i pruža realističke kontakt senke u uglovima i pukotinama.

**Omnidirekcionalne Point Light Senke**  
Cube map shadow mapping pruža 360-stepeno bacanje senki od point light-ova. Uključuje dinamičko bacanje senki od interaktivne lampe sa visoko-rezolucijskim, senke bez artifakata.

**Post-Processing Pipeline**  
HDR-sposobna post-processing pipeline sa tone mapping-om, exposure control-om, gamma korekcijom i atmosferskim efektima. Kreira autentičnu toplu taverna atmosferu sa suptilnim vizuelnim poboljšanjima.

### Karakteristike Scene

**Autentičan Srednjovekovni Layout**  
Bar sto sa pivskim kriglama, tri trpezarijska stola sa osmougaonim stolicama, integracija profesionalnih .obj modela. Visoko-rezolucijska geometrija poda (50x50 podpodela) pruža detaljno primanje senki.

**Interaktivni Sistem Osvetljenja**  
Lampa montirana na kameru sa F tasterom za uključivanje/isključivanje i scroll wheel kontrolom distance. Podešavanje globalnog radius-a svetala sa Q/E tasterima. Dinamička animacija plamena sveća sa realističkim treperenjem.

**Hijerarhija Objekata i Animacija**  
Parent-child odnosi za sveće na stolovima sa animiranim offset-ima plamena. Proceduralno generiranje geometrije za custom taverna elemente uključujući osnove sveća i arhitektonske detalje.

### Kontrole
- **WASD** - Kretanje kamere
- **Miš** - Pogled naokolo
- **F** - Uključ/isključ lampu
- **Scroll Wheel** - Podesi distancu lampe
- **Q/E** - Promeni globalni radius svetala
- **R** - Resetuj distancu lampe

### Prevođenje i Pokretanje
```bash
make clean
make build
make run
```

### Tehnička Implementacija
Izgrađeno sa OpenGL 3.3+ i GLSL 3.30+ koristeći RAFGL framework. Sadrži custom shader programe, proceduralno generiranje geometrije, hijerarhije objekata sa programskim animiranjem, i više tipova tekstura pored osnovnog albedo mapiranja.
