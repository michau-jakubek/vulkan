### Najprostsze użycie - parametr typu `bool`
Załóżmy, że nasz program o nazwie **"program"** pozwala na opcjonalny
i bezargumentowy parametr **"-fps"**, który mówi o włączeniu w programie
wypisywania użytecznej informacji o ilości wyświetlonych ramek w ciągu sekundy.
Czyli chcemy przechwycić wywołanie programu:
```bash
program -fps
```

W ogólnym przypadku najlepiej byłoby zdefiniować opcję:
```cpp
#include "vtfOptionParser.hpp"
using namespace vtf;
constexpr Option optionFPS("-fps", 0);
```
Pierwsze pole strutury `Option`, **const char* name**, jest literałem napisowym porównywanym wprost.
Konwencja, czy będzie to **"-fps"**, czy **"--FPS"** zależy od upodobań i przyzwyczajeń programisty.  
Drugie pole typu **int** informuje, że parametr będzie miał zero argumentów, mówiąc innymi słowy nie
będzie miał żadnych argumentów.

Każdy z przechwytywanych parametrów ma swoje odniesienie do miejsca w pamięci,
gdzie jego wartość będzie przechowywana - i w tym przypadku można to zrobić tak:
```cpp
struct Params
{
    bool fps;
    Params() : fps(false) {}
	void parseCommandLine(add_cref<strings> cmdLineParams);
}; 
```
Na pierwszy rzut oka da się zauważyć dziwoląga "_add_cref\<strings\>_". Jest to skrócony zapis, który
po rozwinięciu wygląda "_const std::vector\<std::string\>\>&_". Więcej o tego typu zabiegach można
przeczytać [tutaj](common_defs.md).

Do parsowania wiersza poleceń można użyć omówionej bardziej szczegółowo później klasy `OptionParser`.
Ciało funkcji parsującej wiersz poleceń mogłoby wyglądac wówczas tak:
```cpp
void Params::parseCommandLine(add_cref<strings> cmdLineParams)
{
    OptionParserState    state;
    OptionFlags          flags (OptionFlag::PrintDefault);
    OptionParser<Params> parser(*this);
	
	parser.addOption(&Params::fps, optionFPS, "Print FPS", { fps }, flags);
	auto unresolvedParams = parser.parse(cmdLineParams);
    if (state = params.getState(); state.hasErrors)
	{
	    throw std::runtime_error(state.messageText());
	}
}
```
Konstruktor klasy `OptionParser` posiada więcej parametrów, ale w najogólniejszym
przypadku jest tworzony na referencji do pola, które będzie wypełniane podczas
wykonywania metody `parse(...)`.  
Metoda `addOption(...)` posiada również więcej parametrów, jednak wymaganymi z nich są:
- wskaźnik do pola pozyskiwanego parametru: **&Params:fps**
- opcja porównywana z wierszem polecenia: **optionFPS**
- tekst do wypisania powiązany z opcją: **"Print FPS"**
- wartość domyślna opcji - tutaj referencja wprost: **{ fps }**
- zestaw flag informujących o zachowaniu się opcji

Po wykonaniu metody `parse(...)` wartość pola **fps** powinna być ustawiona na **true**
o ile w wierszu poleceń **commandLine** był parametr **-fps**.  
W rezultacie metody `parse(...)` otrzymujemy listę nieskonsumowanych i tym samym
nieprzetworzonych parametrów wiersza poleceń. Przez metodę klasy parsera `getOptions()`
można pobrać prostą strukturkę `OptionParserState`
wraz z informacjami o napotkanych broblemach podczas parsowania. W szczegóności
jednym z najczętszych jest niemożność skonsumowania wszystkich parametrów znajdujących
się w wierszu polecenia - informacja o zaistnieniu takiego problemu sygnalizowana jest
poprzez `OptionParserState::hasErrors` i odpowiedni komunikat w `OptionParserState::messages`
możliwy do pobrania w postaci **std::string** poprzez `OptionParserState::messageText()`.

### Pójdźmy dalej - parametr typu `float`
Dodajmy do naszego programu kolejny, również opcjonalny, lecz tym razem wymagający argumentu
typu zmiennopozycyjnego, parametr **"-eps"**. Scenariusz postępowania w kodzie jest zgoła
identyczny, czyli: **1** - definiujemy _opcję_, **2** - dodajemy _pole_, **3** - i obsługujemy
je. Jedyną widoczną różnicą będzie parametr opcji różny od zera, ponieważ **-eps** w przeciwieństwie
do **-fps** wymaga podania argumentu.
```cpp
#include "vtfOptionParser.hpp"
constexpr Option optionEPS("-eps", 1); // parametr wymaga argumentu
struct Params {
    bool  fps;
	float eps;
	Params() : fps(false), eps(0.001f) {}
	// ...
};
void Params::parseCommandLine(add_cref<strings> cmdLineParams)
{
    // ...
    OptionFlags          flags (OptionFlag::PrintDefault);	
    OptionParser<Params> parser(*this);
	parser.addOption(&Params::eps, optionEPS, "Print EPS", { eps }, flags);
	// ...
}
```
Finalnie, z nowoobsłużonym parametrem nasz program możemy uruchomić tak:
```bash
program -eps 0.05 -fps
```

