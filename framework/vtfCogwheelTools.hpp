#ifndef __VTF_COGWHEELTOOLS_HPP_INCLUDED__
#define __VTF_COGWHEELTOOLS_HPP_INCLUDED__

#include "vtfVkUtils.hpp"
#include "vtfVector.hpp"

namespace vtf
{

struct CogwheelDescription
{
	float D;
	uint32_t Z;
	float Ha;
	float Hf;
};

std::vector<Vec2> makeCogwheel (add_cref<CogwheelDescription> cd, VkPrimitiveTopology topology);

/*
* s_t - szerokosc zeba
* e_t - szerokosc wrebu
* p_t - podzialaka zazebienia (s_t + e_t)
* h_a - wysokosc glowy zeba
* h_f - wysokosc stopy seba
* h   - wysokosc zeba (h_a + h_f)
* d_f - srednica stop kola zebatego
* d_a - srednica glow kola zebatego
* d   - srednica podzialowa kola zebatego
* z   - liczba zebow kola zebatego
* alpha - kat przyporu kol zebatych (ok. 20 stopni)
* 
* Parametr p_t, zwany podzialka, mozna obliczyc ze wzoru p_t = (%pi * d) / z.
* Z kolei sama podzialka zeba p_t jest scisle powiazana z modulem zazebienia m,
* ktory mozna obliczy ze wzoru: m = p_t / %pi. Jak mozna zauwazyc, po podstawieniu
* podzialki do wzoru na modul zazebienia, wzor na sam modul upraszcza sie bardzo i
* wyraza on stosunek srednicy kola podzialowego do liczby zebow kola zebatego.
*
* Parametrami szerokosci zeba i wrebu, s_t i e_t sa niczym innym jak odcinkami
* na okregu o srednicy podzialki wyznaczonymi przez przeciecie polprostej
* poprowadzonej od srodka na zewnatrz tego okregu i ewolwenty samego zeba.
* 
* Podczas liczenia dwoch kol zebatych majacych wspolgrac ze soba istotne jest
* dobranie srednicy podzialowej poszczegolnych kol w sposob taki, aby kola te
* obracajac sie bez poslizgu uzyskaly takie samo przelozenie co kola zebate
* calej przekadni. Z zasady tej wynika wzor na przelozenie:
*    i = d_1 / d_2 = z_1 / z2
*/

} // namespace vtf

#endif // __VTF_COGWHEELTOOLS_HPP_INCLUDED__