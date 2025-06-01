#!/usr/bin/env python3
"""
Simple test to verify bit extraction
"""

try:
    import cantools
    
    # Test data
    test_data = bytes([0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80])
    
    print("Testing cantools with data:", " ".join([f"{b:02X}" for b in test_data]))
    
    # Load DBC and decode
    db = cantools.database.load_file('minimal.dbc')
    msg = db.get_message_by_name('BCM_Lamp_Stat_FD1')
    decoded = msg.decode(test_data)
    
    print("Cantools results:")
    for signal_name, value in decoded.items():
        if hasattr(value, 'value'):
            print(f"  {signal_name}: {value.name}({value.value})")
        else:
            print(f"  {signal_name}: {value}")

except Exception as e:
    print(f"Error: {e}")