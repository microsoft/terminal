![terminal-logos](https://github.com/microsoft/terminal/assets/91625426/333ddc76-8ab2-4eb4-a8c0-4d7b953b1179)

[![Terminal Build Status](https://dev.azure.com/shine-oss/terminal/_apis/build/status%2FTerminal%20CI?branchName=main)](https://dev.azure.com/shine-oss/terminal/_build/latest?definitionId=1&branchName=main)

# Windows Terminal, Konsol ve Komut Satırı deposuna hoş geldiniz

<details>
  <summary><strong>İçindekiler</strong></summary>

- [Windows Terminal'i yükleme ve çalıştırma](#installing-and-running-windows-terminal)
  - [Microsoft Store \[Önerilen\]](#microsoft-store-recommended)
  - [Diğer kurulum yöntemleri](#other-install-methods)
    - [GitHub aracılığıyla](#via-github)
    - [Windows Paket Yöneticisi CLI (diğer adıyla winget) aracılığıyla](#via-windows-package-manager-cli-aka-winget)
    - [Chocolatey aracılığıyla (resmi değil)](#via-chocolatey-unofficial)
    - [Scoop aracılığıyla (resmi değil)](#via-scoop-unofficial)
- [Windows Terminal Canary'nin Kurulması](#installing-windows-terminal-canary)
- [Windows Terminal Yol Haritası](#windows-terminal-roadmap)
- [Terminal ve Konsola Genel Bakış](#terminal--console-overview)
  - [Windows Terminal](#windows-terminal)
  - [Windows Konsol Ana Bilgisayarı](#the-windows-console-host)
  - [Paylaşılan Bileşenler](#shared-components)
  - [Yeni Windows Terminali Oluşturma](#creating-the-new-windows-terminal)
- [Kaynaklar](#resources)
- [SSS](#faq)
  - [Yeni Terminali kurdum ve çalıştırdım, ancak tıpkı eski konsol gibi görünüyor](#i-built-and-ran-the-new-terminal-but-it-looks-just-like-the-old-console)
- [Dokümantasyon](#documentation)
- [Katkıda Bulunmak](#contributing)
- [Ekip ile İletişim Kurma](#communicating-with-the-team)
- [Geliştirici Kılavuzu](#developer-guidance)
- [Ön Koşullar](#prerequisites)
- [Kodun Oluşturulması](#building-the-code)
  - [PowerShell'de Oluşturma](#building-in-powershell)
  - [Cmd'de Oluşturma](#building-in-cmd)
- [Çalıştırma ve Hata Ayıklama](#running--debugging)
  - [Kodlama Kılavuzu](#coding-guidance)
- [Davranış Kuralları](#code-of-conduct)

</details>

<br />

Bu depo aşağıdakiler için kaynak kodu içerir:

* [Windows Terminal](https://aka.ms/terminal)
* [Windows Terminal Önizlemesi](https://aka.ms/terminal-preview)
* Windows konsol ana bilgisayarı (`conhost.exe`)
* İki proje arasında paylaşılan bileşenler
* [ColorTool](./src/tools/ColorTool)
* [Örnek projeler](./samples)
  Windows Console API'lerinin nasıl kullanılacağını gösteren

İlgili depolar şunlardır:

* [Windows Terminal Belgeleri](https://docs.microsoft.com/windows/terminal)
  ([Repo: Belgelere katkıda bulunun](https://github.com/MicrosoftDocs/terminal))
* [Konsol API Dokümantasyonu](https://github.com/MicrosoftDocs/Console-Docs)
* [Cascadia Kod Yazı Tipi](https://github.com/Microsoft/Cascadia-Code)

<h3 id="installing-and-running-windows-terminal">Windows Terminal'i yükleme ve çalıştırma</h3>

> [!NOT]
> Windows Terminal için Windows 10 2004 (19041 derlemesi) veya üstü gerekir

<h3 id="microsoft-store-recommended">Microsoft Store [Önerilen]</h3>

[Microsoft Store'dan Windows Terminali][store-install-link] yükleyin.
Bu, otomatik yükseltmelerle yeni derlemeler yayınladığımızda her zaman en son sürümde olmanızı sağlar.

Bu bizim tercih ettiğimiz yöntemdir.

<h3 id="other-install-methods">Diğer kurulum yöntemleri</h3>

<h4 id="via-github">GitHub aracılığıyla</h4>

Windows Terminal'i Microsoft Store'dan yükleyemeyen kullanıcılar için, yayınlanan derlemeler bu deponun [Sürümler sayfası](https://github.com/microsoft/terminal/releases) adresinden manuel olarak indirilebilir.

`Microsoft.WindowsTerminal_<versionNumarası>.msixbundle` dosyasını **Assets** bölümünden indirin. Uygulamayı yüklemek için `.msixbundle` dosyasına çift tıklamanız yeterlidir; uygulama yükleyici otomatik olarak çalışacaktır. Bu herhangi bir nedenle başarısız olursa, bir PowerShell isteminde aşağıdaki komutu deneyebilirsiniz:

```powershell
# NOT: PowerShell 7+ kullanıyorsanız, lütfen çalıştırın
# Import-Module Appx -UseWindowsPowerShell
# kullanmadan önce Add-AppxPackage.

Add-AppxPackage Microsoft.WindowsTerminal_<versionNumarası>.msixbundle
```

> [!NOT]
> Terminal'i manuel olarak yüklerseniz:
>
> * [VC++ v14 Desktop Framework Package](https://docs.microsoft.com/troubleshoot/cpp/c-runtime-packages-desktop-bridge#how-to-install-and-update-desktop-framework-packages). yüklemeniz gerekebilir.
>   Bu yalnızca Windows 10'un eski sürümlerinde ve yalnızca eksik çerçeve paketleriyle ilgili bir hata alırsanız gerekli olmalıdır.
> * Terminal yeni sürümler yayınlandığında otomatik olarak güncellenmeyecektir,
>   bu nedenle en son düzeltmeleri ve iyileştirmeleri almak için düzenli olarak 
>   en son Terminal sürümünü yüklemeniz gerekecektir!

<h4 id="via-windows-package-manager-cli-aka-winget">Windows Paket Yöneticisi CLI (diğer adıyla winget) aracılığıyla</h4>

[winget](https://github.com/microsoft/winget-cli) kullanıcıları `Microsoft.WindowsTerminal` paketini yükleyerek en son Terminal sürümünü indirebilir ve yükleyebilirler:

```powershell
winget install --id Microsoft.WindowsTerminal -e
```

> [!NOT]
> Bağımlılık desteği WinGet sürümünde mevcuttur [1.6.2631 veya üstü](https://github.com/microsoft/winget-cli/releases). Terminal kararlı sürüm 1.18 veya üstünü yüklemek için lütfen WinGet istemcisinin güncellenmiş sürümüne sahip olduğunuzdan emin olun.

<h4 id="via-chocolatey-unofficial">Chocolatey aracılığıyla (resmi değil)</h4>

[Chocolatey](https://chocolatey.org) kullanıcıları `microsoft-windows-terminal` paketini yükleyerek en son Terminal sürümünü indirebilir ve kurabilirler:

```powershell
choco install microsoft-windows-terminal
```

Chocolatey kullanarak Windows Terminal'i yükseltmek için aşağıdakileri çalıştırın:

```powershell
choco upgrade microsoft-windows-terminal
```

Paketi kurarken/yükseltirken herhangi bir sorun yaşarsanız lütfen [Windows Terminal paket sayfası](https://chocolatey.org/packages/microsoft-windows-terminal) adresine gidin ve [Chocolatey triyaj süreci](https://chocolatey.org/docs/package-triage-process) adımlarını izleyin

<h4 id="via-scoop-unofficial">Scoop aracılığıyla (resmi değil)</h4>

[Scoop](https://scoop.sh) kullanıcıları `windows-terminal` paketini yükleyerek en son Terminal sürümünü indirebilir ve kurabilirler:

```powershell
scoop bucket add extras
scoop install windows-terminal
```

Scoop kullanarak Windows Terminal'i güncellemek için aşağıdakileri çalıştırın:

```powershell
scoop update windows-terminal
```

Paketi yüklerken/güncellerken herhangi bir sorunla karşılaşırsanız, lütfen Scoop Ekstralar kovası deposunun [sorunlar sayfasında](https://github.com/lukesampson/scoop-extras/issues) aynı sorunu arayın veya bildirin.

---

<h2 id="installing-windows-terminal-canary">Windows Terminal Canary'nin Kurulması</h2>

Windows Terminal Canary, Windows Terminal'in bir gecelik derlemesidir. Bu yapı, `main` dalımızdaki en son kodu içerir ve size Windows Terminal Preview'a gelmeden önce özellikleri deneme fırsatı verir.

Windows Terminal Canary en az kararlı teklifimizdir, bu nedenle hataları bulma şansımız olmadan önce keşfedebilirsiniz.

Windows Terminal Canary bir Uygulama Yükleyici dağıtımı ve bir Taşınabilir ZIP dağıtımı olarak mevcuttur.

Uygulama Yükleyici dağıtımı otomatik güncellemeleri destekler. Platform sınırlamaları nedeniyle, bu yükleyici yalnızca Windows 11'de çalışır.

Taşınabilir ZIP dağıtımı taşınabilir bir uygulamadır. Otomatik olarak güncellenmez ve güncellemeleri otomatik olarak kontrol etmez. Bu taşınabilir ZIP dağıtımı Windows 10 (19041+) ve Windows 11 üzerinde çalışır.

| Dağıtım  | Mimari    | Bağlantı                                                 |
|---------------|:---------------:|------------------------------------------------------|
| Uygulama Yükleyici | x64, arm64, x86 | [indir](https://aka.ms/terminal-canary-installer) |
| Taşınabilir ZIP  | x64             | [indir](https://aka.ms/terminal-canary-zip-x64)   |
| Taşınabilir ZIP  | ARM64           | [indir](https://aka.ms/terminal-canary-zip-arm64) |
| Taşınabilir ZIP  | x86             | [indir](https://aka.ms/terminal-canary-zip-x86)   |

[Windows Terminal dağıtımlarının türleri](https://learn.microsoft.com/windows/terminal/distributions) hakkında daha fazla bilgi edinin.

---

<h2 id="windows-terminal-roadmap">Windows Terminal Yol Haritası</h2>

Windows Terminali için plan [burada açıklanmıştır](/doc/roadmap-2023.md) ve
proje ilerledikçe güncellenecektir.

<h2 id="terminal--console-overview">Terminal ve Konsola Genel Bakış</h2>

Lütfen koda dalmadan önce aşağıdaki genel bakışı incelemek için birkaç dakikanızı ayırın:

<h3 id="windows-terminal">Windows Terminal</h3>

Windows Terminal, komut satırı kullanıcıları için yeni, modern, zengin özelliklere sahip, üretken bir terminal uygulamasıdır. Windows komut satırı topluluğu tarafından en sık talep edilen sekme desteği, zengin metin, küreselleştirme, yapılandırılabilirlik, temalandırma ve stil oluşturma ve daha birçok özelliği içerir.

Terminalin ayrıca hızlı ve verimli kalmasını ve büyük miktarda bellek veya güç tüketmemesini sağlamak için hedeflerimizi ve önlemlerimizi karşılaması gerekecektir.

<h3 id="the-windows-console-host">Windows Konsol Ana Bilgisayarı</h3>

Windows Console ana bilgisayarı, `conhost.exe`, Windows'un orijinal komut satırı kullanıcı deneyimidir. Ayrıca Windows'un komut satırı altyapısını ve Windows Console API sunucusunu, girdi motorunu, işleme motorunu, kullanıcı tercihlerini vb. barındırır. Bu depodaki konsol ana bilgisayar kodu, Windows'un kendi içindeki `conhost.exe`nin oluşturulduğu asıl kaynaktır.

Ekip, 2014 yılında Windows komut satırının sahipliğini üstlendiğinden bu yana Konsola arka plan şeffaflığı, satır tabanlı seçim, [ANSI / Sanal Terminal dizileri](https://en.wikipedia.org/wiki/ANSI_escape_code), 
[24 bit renk](https://devblogs.microsoft.com/commandline/24-bit-color-in-the-windows-console/),
[Pseudoconsole (“ConPTY”)](https://devblogs.microsoft.com/commandline/windows-command-line-introducing-the-windows-pseudo-console-conpty/)
desteği ve daha fazlası dahil olmak üzere birçok yeni özellik ekledi.

Ancak, Windows Console'un birincil hedefi geriye dönük uyumluluğu sürdürmek olduğundan, topluluğun (ve ekibin) son birkaç yıldır istediği sekmeler, tek kodlu metin ve emoji gibi birçok özelliği ekleyemedik.

Bu sınırlamalar bizi yeni Windows Terminalini yaratmaya yöneltti.

> Genel olarak komut satırının evrimi hakkında daha fazla bilgi edinebilir ve
> Windows komut satırı özellikle [bu blog serisine eşlik eden
> gönderiler](https://devblogs.microsoft.com/commandline/windows-command-line-backgrounder/)
> Command-Line ekibinin blogundan okuyabilirsiniz.

<h3 id="shared-components">Paylaşılan Bileşenler</h3>

Windows Console'u elden geçirirken, kod tabanını önemli ölçüde modernize ettik, mantıksal varlıkları modüllere ve sınıflara temiz bir şekilde ayırdık, bazı önemli genişletilebilirlik noktaları ekledik, birkaç eski, evde yetiştirilen koleksiyon ve kapsayıcıları daha güvenli, daha verimli [STL Konteynerleri](https://docs.microsoft.com/en-us/cpp/standard-library/stl-containers?view=vs-2022) ve Microsoft'un [Windows Uygulama Kütüphaneleri - WIL](https://github.com/Microsoft/wil) kullanarak kodu daha basit ve güvenli hale getirdik.

Bu revizyon, Console'un temel bileşenlerinden birkaçının Windows'taki herhangi bir terminal uygulamasında yeniden kullanılabilmesiyle sonuçlandı. Bu bileşenler arasında yeni bir DirectWrite tabanlı metin düzeni ve işleme motoru, hem UTF-16 hem de UTF-8 depolayabilen bir metin arabelleği, bir VT ayrıştırıcı/verici ve daha fazlası bulunmaktadır.

<h3 id="creating-the-new-windows-terminal">Yeni Windows Terminali Oluşturma</h3>

Yeni Windows Terminal uygulamasını planlamaya başladığımızda, çeşitli yaklaşımları ve teknoloji yığınlarını araştırdık ve değerlendirdik. Nihayetinde hedeflerimize en iyi şekilde C++ kod tabanımıza yatırım yapmaya devam ederek ulaşabileceğimize karar verdik; bu sayede yukarıda bahsedilen modernize edilmiş bileşenlerin birçoğunu hem mevcut Konsolda hem de yeni Terminalde yeniden kullanabileceğiz. Ayrıca, bunun Terminal'in çekirdeğinin çoğunu başkalarının kendi uygulamalarına dahil edebileceği yeniden kullanılabilir bir kullanıcı arayüzü kontrolü olarak oluşturmamıza izin vereceğini fark ettik.

Bu çalışmanın sonucu bu repo içinde yer alır ve
Windows Terminal uygulamasını Microsoft Store'dan veya [doğrudan bu deponun sürümlerinden](https://github.com/microsoft/terminal/releases) indirebilirsiniz.

---

<h2 id="resources">Kaynaklar</h2>

Windows Terminal hakkında daha fazla bilgi için bu kaynaklardan bazılarını yararlı ve ilginç bulabilirsiniz:

* [Komut Satırı Blogu](https://devblogs.microsoft.com/commandline)
* [Komut Satırı Arka Plan Blog Serisi](https://devblogs.microsoft.com/commandline/windows-command-line-backgrounder/)
* Windows Terminal Başlatma: [Terminal “Cızırtı Videosu”](https://www.youtube.com/watch?v=8gw0rXPMMPE&list=PLEHMQNlPj-Jzh9DkNpqipDGCZZuOwrQwR&index=2&t=0s)
* Windows Terminal Başlatma: [Yapı 2019 Oturumu](https://www.youtube.com/watch?v=KMudkRcwjCw)
* Run As Radio: [645. Program - Richard Turner ile Windows Terminali](https://www.runasradio.com/Shows/Show/645)
* Azure Devops Podcasti: [Bölüm 54 - Kayla Cinnamon ve Rich Turner Windows Terminalinde DevOps üzerine](http://azuredevopspodcast.clear-measure.com/kayla-cinnamon-and-rich-turner-on-devops-on-the-windows-terminal-team-episode-54)
* Microsoft Ignite 2019 Oturumu: [Modern Windows Komut Satırı: Windows Terminali - BRK3321](https://myignite.techcommunity.microsoft.com/sessions/81329?source=sessions)

---

<h2 id="faq">SSS</h2>

<h3 id="i-built-and-ran-the-new-terminal-but-it-looks-just-like-the-old-console">Yeni Terminali kurdum ve çalıştırdım, ancak tıpkı eski konsol gibi görünüyor</h3>

Nedeni: Visual Studio'da yanlış çözümü başlatıyorsunuz.

Çözüm: Visual Studio'da `CascadiaPackage` projesini oluşturduğunuzdan ve dağıttığınızdan emin olun.

> [!NOT]
`OpenConsole.exe`, Windows'un komut satırı altyapısını barındıran klasik Windows Konsolu olan yerel olarak oluşturulmuş bir `conhost.exe`dir. OpenConsole, Windows Terminal tarafından ([ConPty](https://devblogs.microsoft.com/commandline/windows-command-line-introducing-the-windows-pseudo-console-conpty/)) aracılığıyla komut satırı uygulamalarına bağlanmak ve bunlarla iletişim kurmak için kullanılır

---

<h2 id="documentation">Dokümantasyon</h2>

Tüm proje belgeleri [aka.ms/terminal-docs](https://aka.ms/terminal-docs) adresinde yer almaktadır. Belgelere katkıda bulunmak isterseniz, lütfen [Windows Terminal Dokümantasyon deposu](https://github.com/MicrosoftDocs/terminal) adresinden bir çekme isteği gönderin.

---

<h2 id="contributing">Katkıda Bulunmak</h2>

Windows Terminal'i oluşturmak ve geliştirmek için siz muhteşem topluluğumuzla birlikte çalışmaktan heyecan duyuyoruz\!

***Bir özellik/düzeltme üzerinde çalışmaya başlamadan önce***, boşa harcanan veya yinelenen çabalardan kaçınmaya yardımcı olmak için lütfen [Katılımcı Kılavuzumuzu](./CONTRIBUTING.md) okuyun ve izleyin.

<h2 id="communicating-with-the-team">Ekip ile İletişim Kurma</h2>

Ekip ile iletişim kurmanın en kolay yolu GitHub sorunlarıdır.

Lütfen yeni sorunlar, özellik istekleri ve önerilerde bulunun, ancak **Yeni bir sorun oluşturmadan önce benzer açık/kapalı önceden var olan sorunları arayın.**

Eğer (henüz) bir sorun teşkil etmediğini düşündüğünüz bir soru sormak isterseniz, lütfen bize Twitter üzerinden ulaşın:

* Christopher Nguyen, Ürün Müdürü:
  [@nguyen_dows](https://twitter.com/nguyen_dows)
* Dustin Howett, Mühendislik Lideri: [@dhowett](https://twitter.com/DHowett)
* Mike Griese, Kıdemli Geliştirici: [@zadjii@mastodon.social](https://mastodon.social/@zadjii)
* Carlos Zamora, Geliştirici: [@cazamor_msft](https://twitter.com/cazamor_msft)
* Pankaj Bhojwani, Geliştirici
* Leonard Hecker, Geliştirici: [@LeonardHecker](https://twitter.com/LeonardHecker)

<h2 id="developer-guidance">Geliştirici Kılavuzu</h2>

<h2 id="prerequisites">Ön Koşullar</h2>

* Windows Terminal'i çalıştırmak için Windows 10 2004 (yapı >= 10.0.19041.0) veya üstünü çalıştırıyor olmanız gerekir
* Windows Terminal'i yerel olarak yüklemek ve çalıştırmak için [Windows Ayarları uygulamasında Geliştirici Modunu etkinleştirmelisiniz](https://docs.microsoft.com/en-us/windows/uwp/get-started/enable-your-device-for-development)
* [PowerShell 7 veya üstü](https://github.com/PowerShell/PowerShell/releases/latest) yüklü olmalıdır
* [Windows 11 (10.0.22621.0) SDK'sının](https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/) yüklü olması gerekir
* En az [VS 2022](https://visualstudio.microsoft.com/downloads/) yüklü olmalıdır
* Aşağıdaki İş Yüklerini VS Installer aracılığıyla yüklemeniz gerekir. Not: Çözümü VS 2022'de açtığınızda [eksik bileşenleri otomatik olarak yüklemenizi isteyecektir](https://devblogs.microsoft.com/setup/configure-visual-studio-across-your-organization-with-vsconfig/):
  * C++ ile Masaüstü Geliştirme
  * Evrensel Windows Platformu Geliştirme
  * **Aşağıdaki Bireysel Bileşenler**
    * C++ (v143) Evrensel Windows Platform Araçları
* Test projeleri oluşturmak için [NET Çerçevesi Hedefleme Paketi](https://docs.microsoft.com/dotnet/framework/install/guide-for-developers#to-install-the-net-framework-developer-pack-or-targeting-pack) yüklemeniz gerekir

<h2 id="building-the-code">Kodun Oluşturulması</h2>

OpenConsole.sln, Visual Studio içinden veya **/tools** dizinindeki bir dizi kolaylık komut dosyası ve araç kullanılarak komut satırından oluşturulabilir:

<h2 id="building-in-powershell">PowerShell'de Oluşturma</h2>

```powershell
Import-Module .\tools\OpenConsole.psm1
Set-MsBuildDevEnvironment
Invoke-OpenConsoleBuild
```

<h2 id="building-in-cmd">Cmd'de Oluşturma</h2>

```shell
.\tools\razzle.cmd
bcz
```

<h2 id="running--debugging">Çalıştırma ve Hata Ayıklama</h2>

VS'de Windows Terminalinde hata ayıklamak için, `CascadiaPackage` (Çözüm Gezgininde) üzerine sağ tıklayın ve özelliklere gidin. Hata ayıklama menüsünde, “Uygulama işlemi” ve “Arka plan görev işlemi ‘ni ’Yalnızca Yerel” olarak değiştirin.

Daha sonra <kbd>F5</kbd> tuşuna basarak Terminal projesini oluşturabilir ve hatalarını ayıklayabilirsiniz. “x64” ya da ‘x86’ platformunu seçtiğinizden emin olun - Terminal ‘Herhangi Cpu’ için oluşturulmaz (çünkü Terminal bir C++ uygulamasıdır, C# değil).

> 👉 WindowsTerminal.exe'yi çalıştırarak Terminal'i doğrudan başlatamazsınız. 
> Bunun nedeni hakkında daha fazla bilgi için
> [#926](https://github.com/microsoft/terminal/issues/926),
> [#4043](https://github.com/microsoft/terminal/issues/4043)

<h3 id="coding-guidance">Kodlama Kılavuzu</h3>

Lütfen kodlama uygulamalarımız hakkında aşağıdaki kısa dokümanları inceleyin.

> 👉 Bu dokümanlarda eksik bir şey bulursanız,
> deponun herhangi bir yerindeki dokümantasyon dosyalarımıza katkıda bulunmaktan çekinmeyin
> (veya yenilerini yazın!)

Projemize etkili bir şekilde katkıda bulunmaları için insanlara neler sağlamamız gerektiğini öğrendikçe bu, devam etmekte olan bir çalışmadır.

* [Kodlama Stili](./doc/STYLE.md)
* [Kod Organizasyonu](./doc/ORGANIZATION.md)
* [Eski kod tabanımızdaki istisnalar](./doc/EXCEPTIONS.md)
* [WIL'de Windows ile arayüz oluşturmak için faydalı akıllı işaretçiler ve makrolar](./doc/WIL.md)

---

<h2 id="code-of-conduct">Davranış Kuralları</h2>

Bu proje [Microsoft Açık Kaynak Davranış Kuralları] [conduct-code] benimsemiştir. Daha fazla bilgi için [Davranış Kuralları SSS][conduct-FAQ] bölümüne bakın veya herhangi bir ek soru veya yorum için [opencode@microsoft.com][conduct-email] ile iletişime geçin.

[davranış kuralları]: https://opensource.microsoft.com/codeofconduct/
[davranış-SSS]: https://opensource.microsoft.com/codeofconduct/faq/
[davranış-email]: mailto:opencode@microsoft.com
[mağaza-yükleme-bağlantısı]: https://aka.ms/terminal
