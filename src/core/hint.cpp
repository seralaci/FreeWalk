#include "core/hint.h"
#include <cmath>
#include <string>

namespace fw {

Hint choose_hint(const HintInput& in) {
  if (!in.enabled || !in.focused || in.paused || in.animating) return Hint::None;
  if (in.driving && in.can_leave) return Hint::GetOut;
  if (in.walking && in.at_door) return Hint::GetIn;
  return Hint::None;
}

std::string hint_text(Hint hint, std::string_view language) {
  if (hint == Hint::None) return {};
  struct Words {
    std::string_view code, get_in, get_out;
  };
  static constexpr Words kWords[] = {
      {"hu", "Beszállás", "Kiszállás"},       {"de", "Einsteigen", "Aussteigen"},     {"fr", "Monter", "Descendre"},
      {"es", "Subir", "Bajar"},               {"it", "Sali", "Scendi"},               {"pl", "Wsiądź", "Wysiądź"},
      {"cs", "Nastoupit", "Vystoupit"},       {"sk", "Nastúpiť", "Vystúpiť"},         {"ro", "Urcă", "Coboară"},
      {"ru", "Сесть в кабину", "Выйти"},      {"uk", "Сісти в кабіну", "Вийти"},      {"pt", "Entrar", "Sair"},
      {"nl", "Instappen", "Uitstappen"},      {"tr", "Bin", "İn"},                    {"sv", "Kliv in", "Kliv ut"},
  };
  // Only the start of the language code counts ("pt_br" and "pt_pt" are both Portuguese).
  const std::string_view code = language.substr(0, language.find('_'));
  for (const Words& w : kWords)
    if (w.code == code) return std::string(hint == Hint::GetIn ? w.get_in : w.get_out);
  return hint == Hint::GetIn ? "Get in" : "Get out";
}

bool truck_parked(const TruckState& truck) {
  return truck.valid && truck.head_known && truck.parking_brake && std::fabs(truck.speed) < kMaxParkedSpeed;
}

}  // namespace fw
