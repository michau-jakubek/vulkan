Gdy zbudujesz projekt z parametrem VTF_AS_DLL=ON sam projekt VTF zostanie zbudowany jako biblioteka dynamiczna.
Dodatkowo zostanie dodany target vtf-launcher budujący się do pliku wykonywalnego, mającego na celu testowanie
wcześniej wspomnianej biblioteki. Podobnie jak w przypadku budowania VTF jako plik wykonywalny funkcjonują
wszystkie parametry aplikacji, które mogą posłużyć do utworzenia instancji i urządzenia Vulkan będącymi do
użycia podczas wywołania natywnego interfejsu VTF znajdującego się w bibliotece.

W katalogu projektu VTF, w podkatalogu app, znajduje się plik vtf_app.hpp zawierający zbiór funkcji umożliwiających
wywołanie interfejsu VTF. Odbywa się to w sztandarowy sposób poprzez załadowanie biblioteki vtf do systemu,
odnalezienie w niej symbolu wyeksportowanej funkcji VTF_GetAPI i wywołanie tej funkcji w celu  uzyskanie wskaźnika
do interfejsu VTF. Scenariusz postępowania jest identyczny jak w przypadku pozyskiwania dostępu do interfejsu
RENDERDOC. Pomimo, iż RENDERDOC oferuje o wiele bogatszy interfejs i służy zupełnie do innych celów, tak w celu
łatwiejszego zrozumienia może posłużyć jako punkt odniesienia dla programistów mających do czynienia z RENDERDOC.

Przykładowy scenariusz na Windows mógłby wyglądać następująco:

void f() {
    VTF_API* vtfapi;
    uitn32_t major, minor, patch, revision;

    HANDLE vtf = GetModuleHandle("vtf.dll");
    pVTF_GetAPI VTF_GetAPI = (pVTF_GetApi)GetProcAddress(vtf, "VTF_GetAPI");
    VTF_GetAPI((void**)vtfapi);
    vtfapi->getVersion(&major, minor, patch, revision);
}

