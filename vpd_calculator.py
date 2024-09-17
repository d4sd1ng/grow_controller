import math

def calculate_vpd(temperature, relative_humidity):
    """
    Calculate Vapor Pressure Deficit (VPD) given temperature and relative humidity.
    
    :param temperature: Temperature in Celsius
    :param relative_humidity: Relative humidity as a percentage
    :return: VPD in kPa
    """
    # Calculate saturation vapor pressure (SVP)
    svp = 0.61078 * math.exp(17.27 * temperature / (temperature + 237.3))
    
    # Calculate actual vapor pressure (AVP)
    avp = svp * (relative_humidity / 100)
    
    # Calculate VPD
    vpd = svp - avp
    
    return round(vpd, 2)

def get_vpd_range(growth_stage):
    """
    Get the ideal VPD range for a given growth stage.
    
    :param growth_stage: 'early_grow', 'late_grow_early_flowering', or 'late_flowering'
    :return: Tuple of (min_vpd, max_vpd) in kPa
    """
    if growth_stage == 'early_grow':
        return (0.4, 0.8)
    elif growth_stage == 'late_grow_early_flowering':
        return (0.8, 1.2)
    elif growth_stage == 'late_flowering':
        return (1.2, 1.6)
    else:
        raise ValueError("Invalid growth stage. Must be 'early_grow', 'late_grow_early_flowering', or 'late_flowering'.")

def is_vpd_in_range(vpd):
    """
    Check if the VPD is within the acceptable range.
    
    :param vpd: VPD value in kPa
    :return: Boolean indicating if VPD is in range
    """
    return 0.4 <= vpd <= 1.6

def calculate_target_humidity(temperature, growth_stage):
    """
    Calculate the target humidity based on the current temperature and growth stage.
    
    :param temperature: Temperature in Celsius
    :param growth_stage: 'early_grow', 'late_grow_early_flowering', or 'late_flowering'
    :return: Target humidity as a percentage
    """
    vpd_min, vpd_max = get_vpd_range(growth_stage)
    vpd_target = (vpd_min + vpd_max) / 2
    
    # Calculate saturation vapor pressure (SVP)
    svp = 0.61078 * math.exp(17.27 * temperature / (temperature + 237.3))
    
    # Calculate target humidity
    target_humidity = ((svp - vpd_target) / svp) * 100
    
    return round(target_humidity)
