==========
PROGRAMFEL
==========

Freeciv 1.14.0 �r en "driftss�ker" utg�va och anses vara tillr�ckligt
fri fr�n programfel f�r dagligt bruk. Om man �nd� hittar ett
programfel skulle vi vilja f� reda p� det s� att vi kan r�tta
det. Denna fil listar k�nda programfel i denna utg�va och ger
information om att anm�la programfel.

Listan inneh�ller endast de tydligaste programfelen. F�r en
fullst�ndig lista, se:

    https://gna.org/bugs/?group=freeciv

K�NDA PROGRAMFEL:
=================

 - I XAW-klienten fungerar inte valm�jligheten -t f�r val av
   rutbildsupps�ttningar. Anv�nd --tiles i st�llet i detta fall.

 - Visningen av enheter i f�ltet under "Avsluta omg�ng"-knappen �r
   lite felaktig. Datorstyrda spelare kan flytta enheter utan att
   f�ltet uppdateras. S� man kan till exempel se 3 - 4 enheter d�r n�r
   det skall vara endast 1. Se �rende PR#2625 f�r ytterligare
   upplysningar (p� engelska).

 - Stadsf�rvaltningsinst�llningarna skickas endast till servern n�r
   man trycker p� "Avsluta omg�ng". �ndringarna som man g�r i
   stadsf�rvaltningsinst�llningarna samma omg�ng som man sparar g�r
   d�rf�r f�rlorade.

 - Om man anv�nder stadsf�rvaltningen s� blir de sparade spelen inte
   endian- och 64-bitarss�kra, s� man kan inte anv�nda filerna p�
   datorer med annan arkitektur.

 - De enkla datorstyrda fienderna �r inte tillr�ckligt enkla f�r
   nyb�rjare. Om de datorstyrda spelarna besegrar dig tidigt i spelet,
   prova att s�tta sververvalm�jligheten "generator" till 2 eller 3,
   det vill s�ga skriv "set generator 2" eller "set generator 3" innan
   spelet har satts ig�ng.

 - De sv�ra datorstyrda fienderna �r inte tillr�ckligt sv�ra f�r
   erfarna spelare. De g�r fortfarande dumma saker, till exempel s�
   f�redrar de att l�mna st�der i upplopp i st�llet f�r att sv�lta ner
   dem.

 - Ibland �r det f�r m�nga framsteg i "M�l"-menyn i
   forskningsredog�relsen s� att menyn str�cker sig utanf�r sk�rmens
   underkant s� att man inte kan v�lja alla framsteg. Detta ber�r
   endast GTK+-1.2-klienten.

 - Alla valm�jligheter som ges till klienten vidarebefordras �ven till
   anv�ndargr�nssnittet. Detta inneb�r att n�r valm�jligheter �r lika
   s� st�r de varandra. Detta orsakar problem f�r valm�jligheten -d
   och i bland -t vid anv�ndande av XAW-klienten. Problemet kan
   undvikas genom att anv�nda de l�nga versionerna i st�llet (--debug
   och --tiles i detta fall). Se �rende PR#1752 f�r ytterligare
   upplysningar (p� engelska).

 - I bland kan man f� meddelandena
   {ss} player for sample <01> not found
	 {ss} player for sample <01> not found
   n�r man anv�nder ljuddrivrutinen esound. Detta �r inget att oroa
   sig f�r.

 - Om man trycker Ctrl+C i klienten n�r man anv�nder ljuddrivrutinen
   esound kan det h�nda att den ljudslinga som f�r n�rvarande spelas
   upp inte avbryts ordentligt.

 - Vissa v�rldsunder b�rjar inte verka f�rr�n omg�ngen efter att de
   har f�rdigst�llts. N�r man till exempel f�rdigst�ller Fyrtornet f�r
   vissa triremer sitt extra drag f�rst n�sta omg�ng.

 - XAW-klienten kan endast visa 25 st�der i stadsredog�relsen.

 - Sj�lvst�ndigt angrepp fungerar i allm�nhet inte bra.

 - N�r en f�rflyttningsv�g planeras i servern, till exempel f�r en
   sj�lvst�ndig bos�ttare eller ett flygplan, anv�nds information som
   inte �r tillg�nglig f�r spelaren.

 - Forskningsdialogen uppdateras inte n�r man l�r sig en teknologi.
   Man m�ste st�nga och �ppna den.

 - I GTK+-klienten f�rekommer i bland skr�p i omr�det n�ra den lilla
   kartan.

 - Triremer hanteras inte bra i sj�lvst�ndigt l�ge.

 - LOG_DEBUG fungerar inte med andra kompilatorer �n GCC.

 - N�r man s�tter servervariabler kontrollerar servern ofta inte
   v�rdena s� bra som den skulle kunna.

 - D�liga saker h�nder om man �ndrar flera �vergripande arbetslistor
   samtidigt.

 - �ven i spel d�r datormotst�ndare �r de enda motst�ndarna f�r de
   m�jlighet att utf�ra drag b�de f�re och efter den m�nskliga
   spelaren varje omg�ng. Detta ger i bland sken av att datorn g�r 2
   drag.

 - XAW-klienten fungerar inte bra ben KDEs f�nsterhanterare. F�rs�k
   med GTK+-klienten eller en annan f�nsterhanterare.

ANM�LA PROGRAMFEL
=================

Om det �r ett fel i en �vers�ttning ska det anm�las till �vers�ttaren
f�r spr�ket i fr�ga.
Se <http://freeciv.org/wiki/Translations> f�r namn p� och epostadresser
till �vriga �vers�ttare.

S� h�r g�r man:

- Ser efter att det inte �r n�got av programfelen i listan ovan! :-)

- Tittar p� <http://www.freeciv.org> och f�rs�krar sig om att man har
  den nyaste versionen. (Vi kanske redan har r�ttat felet.)

  Man kanske vill prova en utvecklarversion fr�n CVS-upplaget. De kan
  h�mtas fr�n:

        http://download.gna.org/freeciv/latest/

- Tittar p� Freecivs FAQ p� <http://www.freeciv.org/faq> f�r att se om
  n�got s�tt att upph�va problemet har offentliggjorts.

- Tittar p� Freecivs system f�r sp�rning av programfel vid:

        https://gna.org/bugs/?group=freeciv

  f�r att se om programfelet redan har anm�lts.

- Anm�ler programfelet

   Om man vill skicka f�rslag till freecivutvecklarna utan att g�ra en
   programfelsanm�lan kan man skicka epost till
   <freeciv-dev@gna.org>, freecivutvecklarnas s�ndlista.

   Vad man ska n�mna i sin programfelsanm�lan:

   - Beskrivning av problemet och i f�rekommande fall det felmeddeland
     man f�r.

   - Vilken klient man anv�nder (Gtk+ eller Xaw).

   - Namn och versionsnummer:

       - Det operativsystem som man anv�nder. Kommandot "uname -a" kan
         vara anv�ndbart.

       - Versionsnumret f�r Freeciv.

       - Om man anv�nder Gtk+-klienten, versionsnumren (om man k�nner
         till dem) f�r sina Gtk+-, glib- och imlibbibliotek.

       - Om man anv�nder Xaw-klienten, versionsnumren (om man k�nner
         till dem) f�r X-biblioteken, Xpm-biblioteket och
         Xaw-biblioteket och i synnerhet om det �r en variant s�som
         Xaw3d, Xaw95 eller Nextaw.

       - Om man kompilerar fr�n k�llkod, namnet och versionsnumret f�r
         kompilatorn.

       - Om man installerar fr�n ett f�rdigkompilerat paket, dess
         namn, vilken distribution det �r f�r och varifr�n man h�mtat
         det.

   - Om Freeciv "dumpar core", kan vi efterfr�ga en "stacksp�rning",
     vilken ges vid anv�ndning av en avlusare. F�r detta beh�vs
     "core"-filen, s� var god beh�ll den ett tag.

YTTERLIGARE INFORMATION:
========================

F�r mer information se Freecivs plats p� v�rldsv�ven:

        http://www.freeciv.org/
