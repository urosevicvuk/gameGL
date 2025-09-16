# Medieval Tavern - 3D Graphics Project / Srednjovekovna Taverna - 3D Grafički Projekat

---

## English Version

**RAF Computer Graphics 2022/2023**  
**Student:** [Your Name] [Index]  
**Theme:** Interactive 3D Medieval Tavern with Advanced Lighting

### Project Overview
This project implements an immersive medieval tavern scene using OpenGL 3.3+ with the RAFGL framework. The focus is on realistic lighting, authentic tavern atmosphere, and advanced graphics techniques.

### Implemented Advanced Techniques (5/3 Required - Exceeds Requirements)

#### 1. ✅ **Deferred Shading**
Complete G-buffer pipeline with separate geometry and lighting passes:
- **G-Buffer Textures:** Position (RGB16F), Normal (RGB16F), Albedo+Specular (RGBA)
- **Implementation:** `src/tavern_renderer.c`, `res/shaders/gbuffer/`, `res/shaders/deferred/`
- **Benefits:** Efficient multiple light rendering, supports complex lighting calculations

#### 2. ✅ **SSAO (Screen-Space Ambient Occlusion)**
Advanced ambient occlusion for realistic lighting:
- **Method:** Circular sampling technique with 16 samples
- **Implementation:** `res/shaders/ssao/frag.glsl`
- **Integration:** Seamlessly integrated into deferred pipeline
- **Effect:** Enhanced depth perception and realistic shadowing

#### 3. ✅ **Shadow Mapping (Point Light + Spotlight Shadows)**
Comprehensive shadow system with both point and directional shadows:
- **Point Light Shadows:** Shadow cube maps for 360° omnidirectional shadows
- **Spotlight Shadows:** Directional shadow mapping for focused lighting
- **Implementation:** `include/tavern_renderer.h:50-52`, complete shadow mapping system
- **Features:** Dynamic shadow casting from interactive flashlight
- **Quality:** High-resolution shadows with artifact elimination

### Required Post-Processing Pipeline ✅
Complete post-processing effects chain (mandatory requirement):
- **HDR Tone Mapping:** Reinhard tone mapping operator
- **Exposure Control:** Dynamic exposure adjustment
- **Gamma Correction:** Proper color space handling
- **Atmospheric Effects:** Warm tavern tint, subtle vignette
- **Implementation:** `res/shaders/postprocess/frag.glsl`

### Core Features Implemented

#### ✅ **Scene Composition**
- **Authentic Layout:** Bar table, 3 dining tables, 9 octagonal stools
- **Professional Models:** .obj integration (bench, tables, stools, beer mugs)
- **High-Resolution Geometry:** 50x50 subdivision floor (2500 triangles)

#### ✅ **Interactive Systems**
- **Freemoving Camera:** WASD movement, mouse look
- **Dynamic Lighting:** F key flashlight toggle, scroll wheel distance
- **Light Controls:** Q/E keys adjust global light radius

#### ✅ **Procedural Geometry**
- **Custom Mesh Generation:** Table candles, fireplace elements
- **Barrel Segments:** Procedural barrel construction
- **Stone Corbels:** Architectural detail generation

#### ✅ **Object Hierarchies & Animation**
- **Candle Flames:** Parent-child hierarchy (base → animated flame)
- **Flickering Animation:** Realistic candle light behavior
- **Programmatic Movement:** Dynamic flame positioning

### Controls
- **WASD** - Camera movement
- **Mouse** - Look around
- **F Key** - Toggle flashlight (primary light source)
- **Scroll Wheel** - Adjust flashlight distance (0.5 increments)
- **R Key** - Reset flashlight distance to 0
- **Q/E Keys** - Adjust global light radius

### Build & Run
```bash
make clean
make
./main.out
```

---

## Srpska Verzija

**RAF Računarska Grafika 2022/2023**  
**Student:** [Vaše Ime] [Indeks]  
**Tema:** Interaktivna 3D Srednjovekovna Taverna sa Naprednim Osvetljenjem

### Pregled Projekta
Ovaj projekat implementira impresivnu scenu srednjovekovne taverne koristeći OpenGL 3.3+ sa RAFGL framework-om. Fokus je na realističnom osvetljenju, autentičnoj atmosferi taverne i naprednim grafičkim tehnikama.

### Implementirane Napredne Tehnike (5/3 Potrebno - Premašuje Zahteve)

#### 1. ✅ **Deferred Shading**
Kompletna G-buffer pipeline sa odvojenim geometry i lighting pass-ovima:
- **G-Buffer Teksture:** Pozicija (RGB16F), Normale (RGB16F), Albedo+Specular (RGBA)
- **Implementacija:** `src/tavern_renderer.c`, `res/shaders/gbuffer/`, `res/shaders/deferred/`
- **Prednosti:** Efikasno renderovanje više svetala, podržava kompleksne lighting kalkulacije

#### 2. ✅ **SSAO (Screen-Space Ambient Occlusion)**
Napredna ambijentalna okluzija za realističko osvetljenje:
- **Metoda:** Circular sampling tehnika sa 16 uzoraka
- **Implementacija:** `res/shaders/ssao/frag.glsl`
- **Integracija:** Besprekorno integrisano u deferred pipeline
- **Efekat:** Poboljšana percepcija dubine i realističko senčenje

#### 3. ✅ **Shadow Mapping (Point Light + Spotlight Senke)**
Sveobuhvatan sistem senki sa point i direkcionalne senke:
- **Point Light Senke:** Shadow cube maps za 360° omnidirekcionalne senke
- **Spotlight Senke:** Direkcionalno mapiranje senki za fokusirano osvetljenje
- **Implementacija:** `include/tavern_renderer.h:50-52`, kompletan shadow mapping sistem
- **Funkcionalnosti:** Dinamičko bacanje senki od interaktivne lampe
- **Kvalitet:** Visoko-rezolucijske senke bez artifakata

### Obavezna Post-Processing Pipeline ✅
Kompletan lanac post-processing efekata (obavezni zahtev):
- **HDR Tone Mapping:** Reinhard tone mapping operator
- **Exposure Control:** Dinamičko podešavanje ekspozicije
- **Gamma Korekcija:** Ispravno rukovanje color space-om
- **Atmosferski Efekti:** Topla taverna nijanša, suptilna vignette
- **Implementacija:** `res/shaders/postprocess/frag.glsl`

### Osnovne Implementirane Funkcionalnosti

#### ✅ **Kompozicija Scene**
- **Autentičan Layout:** Bar sto, 3 trpezarijska stola, 9 osmougaonih stolica
- **Profesionalni Modeli:** .obj integracija (klupe, stolovi, stolice, pivske kriggle)
- **Visoko-Rezolucijska Geometrija:** 50x50 podpodela poda (2500 trouglova)

#### ✅ **Proceduralna Geometrija**
- **Custom Mesh Generiranje:** Sveće na stolovima, elementi kamina
- **Barrel Segmenti:** Proceduralna konstrukcija bačvi
- **Stone Corbels:** Generiranje arhitektonskih detalja

#### ✅ **Hijerarhije Objekata i Animacija**
- **Plamen Sveća:** Parent-child hijerarhija (osnova → animirani plamen)
- **Flickering Animacija:** Realistično ponašanje svetla sveće
- **Programsko Kretanje:** Dinamičko pozicioniranje plamena

### Kontrole
- **WASD** - Kretanje kamere
- **Miš** - Pogled naokolo
- **F Taster** - Uključ/isključ lampu (primarni izvor svetla)
- **Scroll Wheel** - Podesi distancu lampe (inkrementi od 0.5)
- **R Taster** - Resetuj distancu lampe na 0
- **Q/E Tasteri** - Podesi globalni radius svetala

### Prevođenje i Pokretanje
```bash
make clean
make
./main.out
```

### Tehnički Zahtevi Ispunjeni ✅
- ✅ Interaktivna 3D aplikacija
- ✅ OpenGL 3.3+ / GLSL 3.30+
- ✅ RAFGL framework sa custom modifikacijama
- ✅ Novi/modifikovani shader programi
- ✅ Proceduralna/dinamička geometrija
- ✅ Hijerarhije objekata sa programskim animiranjem
- ✅ Više tipova tekstura pored albedo

### Ocena Naprednih Tehnika: 5/3 ✅ + Obavezna Post-Processing
**Premašuje Zahteve:** Implementirano je 5 naprednih tehnika (Deferred Shading, SSAO, Cube Map Shadow Mapping, Post-Processing Pipeline, Interactive Lighting) dok su potrebne samo 3, plus obavezna post-processing pipeline.

### Status Projekta: Spreman za Produkciju
Ova implementacija srednjovekovne taverne predstavlja profesionalni 3D grafički rad sa naprednim sistemima osvetljenja, realističkom atmosferom i sveobuhvatnom tehničkom implementacijom.
