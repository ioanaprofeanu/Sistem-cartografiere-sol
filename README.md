# Sistem Cartografiere sol

Acesta este proiectul meu la Proiectarea cu Microprocesoare. Este un sistem inteligent de cartografiere a solului, ce este capabil sa afiseze metricile de mediu (recalculate la o ora), metricile medii, precum si sa afiseze pe ecran o lista cu plantele ce pot fi plantate in acel mediu.

Proiectul utilizeaza urmatoarele tehnologii:

- Intreruperi hardware (pentru butonul ce controleaza meniul)
- Software (timer)
- Software Serial pentru a obtine datele de la modulul GPS (din care extrage latitudinea, pentru a obtine mai apoi clima)
- SPI pentru citirea si scrierea datelor pe un card SD
- I2C pentru conexiunea ecranului OLED
- ADC pentru citirea datelor de pe senzorii analogici (de temperatura, de umiditate sol si fotorezistor)

Pagina proiectului poate fi accesata la urmatorul link: [Sistem Cartografiere Sol](https://ocw.cs.pub.ro/courses/pm/prj2023/adarmaz/sistem-cartografiere-sol)
