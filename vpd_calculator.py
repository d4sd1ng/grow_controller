import math

# Referenz: vom Nutzer bereitgestellte VPD-Tabelle (Blatt 2 C unter Luft).
LEAF_AIR_OFFSET_C = 2.0


def _saturation_vapor_pressure_kpa(temp_c):
    """Saettigungsdampfdruck nach Tetens (kPa)."""
    return 0.61078 * math.exp(17.27 * temp_c / (temp_c + 237.3))


def calculate_vpd(temperature, relative_humidity, leaf_temperature=None):
    """Blatt-VPD in kPa.

    Bis eine IR-Blattmessung verifiziert ist, wird T_leaf=T_air-2 C
    gemaess der bereitgestellten VPD-Tabelle verwendet.
    Eine explizit angegebene Blatt-Temperatur ersetzt die -2-C-Annahme.
    """
    leaf_c = temperature - LEAF_AIR_OFFSET_C if leaf_temperature is None else leaf_temperature
    actual_vapor_air = _saturation_vapor_pressure_kpa(temperature) * (relative_humidity / 100.0)
    return round(_saturation_vapor_pressure_kpa(leaf_c) - actual_vapor_air, 2)

def get_vpd_range(growth_stage):
    """
    Get the ideal VPD range and humidity range for a given growth stage.

    :param growth_stage: 'early_grow', 'late_grow_early_flowering', or 'late_flowering'
    :return: Tuple of (min_vpd, max_vpd, min_humidity, max_humidity)
    """
    if growth_stage == 'early_grow':
        return (0.4, 0.8, 60, 70)
    elif growth_stage == 'late_grow_early_flowering':
        return (0.8, 1.2, 50, 60)
    elif growth_stage == 'late_flowering':
        return (1.2, 1.6, 40, 50)
    else:
        raise ValueError("Invalid growth stage. Must be 'early_grow', 'late_grow_early_flowering', or 'late_flowering'.")

def is_vpd_in_range(vpd, growth_stage=None):
    """
    Check if VPD is in range. If growth_stage is set, uses that stage's band from get_vpd_range;
    otherwise uses the overall 0.4–1.6 kPa envelope.
    """
    if growth_stage is None:
        return 0.4 <= vpd <= 1.6
    vpd_min, vpd_max, _, _ = get_vpd_range(growth_stage)
    return vpd_min <= vpd <= vpd_max

def calculate_target_humidity(temperature, growth_stage):
    """
    Calculate the target humidity based on the current temperature and growth stage.

    :param temperature: Temperature in Celsius
    :param growth_stage: 'early_grow', 'late_grow_early_flowering', or 'late_flowering'
    :return: Target humidity as a percentage
    """
    vpd_min, vpd_max, humidity_min, humidity_max = get_vpd_range(growth_stage)
    vpd_target = (vpd_min + vpd_max) / 2

    # Ziel-RH aus demselben -2-C-Blattmodell wie die VPD-Tabelle.
    svp_air = _saturation_vapor_pressure_kpa(temperature)
    svp_leaf = _saturation_vapor_pressure_kpa(temperature - LEAF_AIR_OFFSET_C)
    target_humidity = ((svp_leaf - vpd_target) / svp_air) * 100

    # Der VPD-Zielwert hat Vorrang vor pauschalen Feuchte-Korridoren.
    # Lediglich physikalisch moegliche 0..100 % begrenzen.
    target_humidity = max(0.0, min(100.0, target_humidity))

    return round(target_humidity)
