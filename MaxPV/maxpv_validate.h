/*
  This file is a part of MaxPV.ino - ESP8266 program that provides a web interface and a API for EcoPV
  Copyright (C) 2022 - Bernard Legrand.
  maxpv@bernard-legrand.net
  https://github.com/Jetblack31/

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation, either version 2.1 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#pragma once

#include <string.h>

// Valeur numérique d'un paramètre EcoPV destinée à SETPARAM : "-" optionnel,
// chiffres, au plus un ".", 1 à 15 caractères (32 octets de commande moins
// "SETPARAM,NN," et ",END#"). Aucun séparateur série (",", "#") ne passe.
inline bool isNumericParamValue(const char *s) {
  if (s == nullptr) return false;
  size_t len = strlen(s);
  if (len < 1 || len > 15) return false;
  bool digit = false, dot = false;
  for (size_t i = 0; i < len; i++) {
    char c = s[i];
    if (c >= '0' && c <= '9') digit = true;
    else if (c == '.' && !dot) dot = true;
    else if (c == '-' && i == 0) continue;
    else return false;
  }
  return digit;
}
