#!/usr/bin/env python3
"""
CAN Bus Analyzer for Vehicle Action Detection

This script helps identify CAN messages related to specific vehicle actions like
door locks or light controls. It captures CAN bus data before, during, and after
specific actions, then analyzes the differences to identify relevant messages.

Features:
- Capture baseline traffic for comparison
- Capture traffic during specific vehicle actions
- Option to repeat actions multiple times for higher confidence
- Compare captures to identify relevant messages
- Visualize message timing with markers for when actions were performed
- Save and load captures for later analysis

Usage:
1. Install dependencies: pip install -r requirements.txt
2. Set up Waveshare CAN HAT on Raspberry Pi and configure SocketCAN
3. Run the script: python can_action_analyzer.py <can_interface>

Examples:
    python can_action_analyzer.py can0
    python can_action_analyzer.py slcan0
    python can_action_analyzer.py --help

The script will guide you through the process of capturing and analyzing CAN data.
"""

import can
import time
import os
import json
import numpy as np
import matplotlib.pyplot as plt
from datetime import datetime
from collections import defaultdict
import argparse
import sys


class CANActionAnalyzer:
    def __init__(self, can_interface, sample_rate=0.001):
        """
        Initialize the CAN bus analyzer.

        Args:
            can_interface: CAN interface name (required)
            sample_rate: Time between CAN message reads in seconds (default: 0.001)
        """
        self.can_interface = can_interface
        self.sample_rate = sample_rate
        self.bus = None
        self.captures = {}
        self.sessions_dir = "can_sessions"
        
        # Create directory for saving captured data
        if not os.path.exists(self.sessions_dir):
            os.makedirs(self.sessions_dir)
            
        print(f"CAN Action Analyzer initialized on interface: {can_interface}")
        print(f"Data will be saved to: {os.path.abspath(self.sessions_dir)}")

    def connect(self):
        """Connect to the CAN bus interface."""
        try:
            self.bus = can.interface.Bus(
                channel=self.can_interface,
                bustype='socketcan'
            )
            print(f"Successfully connected to {self.can_interface}")
            return True
        except Exception as e:
            print(f"Error connecting to CAN bus: {e}")
            return False

    def disconnect(self):
        """Disconnect from the CAN bus interface."""
        if self.bus:
            self.bus.shutdown()
            print(f"Disconnected from {self.can_interface}")

    def capture_messages(self, duration, action_name="baseline", show_progress=True, repeat_count=1, repeat_interval=5):
        """
        Capture CAN messages for a specified duration.

        Args:
            duration: Duration to capture in seconds
            action_name: Name of the action being performed (for labeling)
            show_progress: Whether to show a progress indicator
            repeat_count: Number of times to repeat the action during capture
            repeat_interval: Time in seconds between repeated actions

        Returns:
            List of captured CAN messages
        """
        if not self.bus:
            if not self.connect():
                return []

        # Calculate total capture duration based on repeats
        total_duration = duration
        if repeat_count > 1 and action_name != "baseline":
            total_duration = duration + (repeat_count - 1) * repeat_interval
            print(f"\nCapturing {action_name} CAN traffic for {total_duration} seconds with {repeat_count} repeats...")
        else:
            print(f"\nCapturing {action_name} CAN traffic for {duration} seconds...")
        
        if action_name != "baseline":
            print("Please perform the action when prompted...")
        
        # Countdown before starting capture
        for i in range(3, 0, -1):
            print(f"Starting capture in {i}...")
            time.sleep(1)
        
        # For baseline, no action prompts needed
        if action_name == "baseline":
            print("Recording baseline traffic. Keep the vehicle idle...")
        else:
            print(f"NOW! Perform the '{action_name}' action (1 of {repeat_count})!")
        
        # Capture messages
        messages = []
        action_timestamps = []  # Store when each action was performed
        start_time = time.time()
        end_time = start_time + total_duration
        next_action_time = start_time + duration  # Time for next action prompt
        action_count = 1
        
        try:
            while time.time() < end_time:
                current_time = time.time()
                
                # Prompt for next action if needed
                if action_name != "baseline" and repeat_count > 1 and action_count < repeat_count and current_time >= next_action_time:
                    action_count += 1
                    print(f"\nNOW! Perform the '{action_name}' action again ({action_count} of {repeat_count})!")
                    action_timestamps.append(current_time - start_time)
                    next_action_time = current_time + repeat_interval
                
                msg = self.bus.recv(timeout=self.sample_rate)
                if msg:
                    # Store message with timestamp relative to start
                    msg.timestamp = current_time - start_time
                    messages.append(msg)
                
                # Show progress
                if show_progress and len(messages) % 100 == 0:
                    elapsed = current_time - start_time
                    progress = int((elapsed / total_duration) * 100)
                    print(f"\rProgress: {progress}% ({len(messages)} messages)", end="")
                    
        except KeyboardInterrupt:
            print("\nCapture interrupted by user.")
        
        print(f"\nCapture complete. Collected {len(messages)} messages across {action_count} action(s).")
        
        # Store the capture with the action name
        self.captures[action_name] = messages
        
        # Store action timestamps as metadata in the capture
        if action_name != "baseline" and repeat_count > 1:
            # Create a simple metadata message at the start
            if messages:
                meta_msg = type('Message', (), {})()
                meta_msg.timestamp = 0
                meta_msg.arbitration_id = 0
                meta_msg.dlc = 8
                meta_msg.data = bytes([0, 0, 0, 0, 0, 0, 0, 0])
                meta_msg.is_extended_id = False
                meta_msg.is_remote_frame = False
                meta_msg.is_error_frame = False
                meta_msg.action_timestamps = [0] + action_timestamps  # Add first action at time 0
                messages.insert(0, meta_msg)
                print(f"Recorded timestamps for {action_count} actions.")
        
        return messages

    def save_capture(self, action_name):
        """
        Save a specific capture to a file.

        Args:
            action_name: Name of the capture to save
        """
        if action_name not in self.captures:
            print(f"No capture found with name '{action_name}'")
            return
            
        # Convert CAN messages to serializable format
        serializable_msgs = []
        for msg in self.captures[action_name]:
            msg_dict = {
                'timestamp': msg.timestamp,
                'arbitration_id': msg.arbitration_id,
                'dlc': msg.dlc,
                'data': list(msg.data),
                'is_extended_id': msg.is_extended_id,
                'is_remote_frame': msg.is_remote_frame,
                'is_error_frame': msg.is_error_frame,
            }
            # Add action timestamps if they exist (for multi-action captures)
            if hasattr(msg, 'action_timestamps'):
                msg_dict['action_timestamps'] = msg.action_timestamps
                
            # Add binary toggle metadata if they exist
            if hasattr(msg, 'toggle_events'):
                msg_dict['toggle_events'] = msg.toggle_events
            if hasattr(msg, 'initial_state'):
                msg_dict['initial_state'] = msg.initial_state
            if hasattr(msg, 'toggled_state'):
                msg_dict['toggled_state'] = msg.toggled_state
            if hasattr(msg, 'capture_type'):
                msg_dict['capture_type'] = msg.capture_type
                
            serializable_msgs.append(msg_dict)
            
        # Create filename with timestamp
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"{self.sessions_dir}/{timestamp}_{action_name}.json"
        
        # Save to file
        with open(filename, 'w') as f:
            json.dump(serializable_msgs, f, indent=2)
            
        print(f"Saved {len(serializable_msgs)} messages to {filename}")
        
    def load_capture(self, filename):
        """
        Load a capture from a file.

        Args:
            filename: Path to the capture file
        
        Returns:
            Name of the loaded capture (added to self.captures)
        """
        with open(filename, 'r') as f:
            data = json.load(f)
            
        # Extract action name from filename
        base = os.path.basename(filename)
        action_name = base.split('_', 1)[1].rsplit('.', 1)[0]
        
        # Convert serialized data back to Message-like objects
        messages = []
        for msg_data in data:
            msg = type('Message', (), {})()
            msg.timestamp = msg_data['timestamp']
            msg.arbitration_id = msg_data['arbitration_id']
            msg.dlc = msg_data['dlc']
            msg.data = bytes(msg_data['data'])
            msg.is_extended_id = msg_data['is_extended_id']
            msg.is_remote_frame = msg_data['is_remote_frame']
            msg.is_error_frame = msg_data['is_error_frame']
            
            # Load action timestamps if they exist
            if 'action_timestamps' in msg_data:
                msg.action_timestamps = msg_data['action_timestamps']
                
            # Load binary toggle metadata if they exist
            if 'toggle_events' in msg_data:
                msg.toggle_events = msg_data['toggle_events']
            if 'initial_state' in msg_data:
                msg.initial_state = msg_data['initial_state']
            if 'toggled_state' in msg_data:
                msg.toggled_state = msg_data['toggled_state']
            if 'capture_type' in msg_data:
                msg.capture_type = msg_data['capture_type']
                
            messages.append(msg)
            
        self.captures[action_name] = messages
        
        # Detect what kind of capture this is
        capture_type = "Regular"
        action_count = 1
        
        if messages:
            if hasattr(messages[0], 'capture_type') and messages[0].capture_type == 'binary_toggle':
                capture_type = "Binary Toggle"
                if hasattr(messages[0], 'toggle_events'):
                    action_count = len(messages[0].toggle_events)
            elif hasattr(messages[0], 'action_timestamps'):
                action_count = len(messages[0].action_timestamps)
                capture_type = "Multi-Action"
        
        if capture_type == "Binary Toggle":
            print(f"Loaded {len(messages)} messages for '{action_name}' - Binary Toggle with {action_count} state changes")
        elif action_count > 1:
            print(f"Loaded {len(messages)} messages for '{action_name}' with {action_count} action repeats")
        else:
            print(f"Loaded {len(messages)} messages for '{action_name}'")
            
        return action_name

    def compare_captures(self, action_name, baseline_name="baseline"):
        """
        Compare an action capture with a baseline to identify differences.

        Args:
            action_name: Name of the action capture to analyze
            baseline_name: Name of the baseline capture to compare against
            
        Returns:
            Dictionary of message IDs and their differences
        """
        if action_name not in self.captures:
            print(f"Action capture '{action_name}' not found")
            return None
            
        if baseline_name not in self.captures:
            print(f"Baseline capture '{baseline_name}' not found")
            return None
            
        action_messages = self.captures[action_name].copy()
        baseline_messages = self.captures[baseline_name]
        
        # Check if this is a multi-action capture by looking for metadata
        action_timestamps = []
        if action_messages and hasattr(action_messages[0], 'action_timestamps'):
            action_timestamps = action_messages[0].action_timestamps
            # Remove the metadata message from analysis
            action_messages = action_messages[1:]
        
        # Group messages by ID
        action_by_id = defaultdict(list)
        baseline_by_id = defaultdict(list)
        
        for msg in action_messages:
            action_by_id[msg.arbitration_id].append(msg)
            
        for msg in baseline_messages:
            baseline_by_id[msg.arbitration_id].append(msg)
            
        # Analyze differences
        differences = {}
        all_ids = set(action_by_id.keys()) | set(baseline_by_id.keys())
        
        for msg_id in all_ids:
            base_count = len(baseline_by_id[msg_id])
            action_count = len(action_by_id[msg_id])
            
            # Calculate message frequency (messages per second)
            base_freq = base_count / (baseline_messages[-1].timestamp if baseline_messages else 1)
            action_freq = action_count / (action_messages[-1].timestamp if action_messages else 1)
            
            # Check for frequency changes
            freq_change = action_freq - base_freq
            freq_change_pct = (freq_change / base_freq * 100) if base_freq > 0 else float('inf')
            
            # Identify new or different messages
            is_new = base_count == 0 and action_count > 0
            has_freq_change = abs(freq_change_pct) > 20  # 20% threshold
            
            # Check for data pattern changes
            data_changed = False
            # Convert bytearray to immutable bytes object before adding to set
            unique_payloads_base = set(bytes(msg.data) for msg in baseline_by_id[msg_id])
            unique_payloads_action = set(bytes(msg.data) for msg in action_by_id[msg_id])
            new_payloads = unique_payloads_action - unique_payloads_base
            
            # Check for correlation with action timestamps if multi-action capture
            action_correlated = False
            if action_timestamps and len(action_timestamps) > 1 and msg_id in action_by_id:
                # Get message timestamps
                msg_timestamps = [msg.timestamp for msg in action_by_id[msg_id]]
                
                # Check if messages occur close to action timestamps
                correlation_count = 0
                window = 1.0  # 1 second window around each action
                
                for action_time in action_timestamps:
                    for msg_time in msg_timestamps:
                        if abs(msg_time - action_time) < window:
                            correlation_count += 1
                            break
                
                # Consider correlated if messages appear near most action timestamps
                if correlation_count >= len(action_timestamps) * 0.5:  # At least 50% of actions
                    action_correlated = True
            
            if is_new or has_freq_change or new_payloads:
                differences[msg_id] = {
                    'msg_id': hex(msg_id),
                    'is_new': is_new,
                    'base_count': base_count,
                    'action_count': action_count,
                    'freq_change_pct': freq_change_pct,
                    'new_payloads': [list(payload) for payload in new_payloads],
                    'baseline_payloads': [list(payload) for payload in unique_payloads_base],
                    'action_payloads': [list(payload) for payload in unique_payloads_action],
                    'action_name': action_name,
                    'action_correlated': action_correlated,
                }
                
        return differences

    def print_differences(self, differences):
        """
        Print the differences between captures in a readable format.

        Args:
            differences: Dictionary of differences from compare_captures
        """
        if not differences:
            print("No significant differences found")
            return
            
        print(f"\n{'=' * 80}")
        print(f"ANALYSIS RESULTS - Found {len(differences)} potential messages of interest")
        print(f"{'=' * 80}")
        
        # Sort by likelihood of being related to the action
        sorted_diffs = sorted(
            differences.items(),
            key=lambda x: (
                x[1].get('action_correlated', False),  # Action correlated messages first
                x[1]['is_new'],  # Then new messages
                len(x[1]['new_payloads']),  # Then by number of new payloads
                abs(x[1]['freq_change_pct'])  # Then by frequency change
            ),
            reverse=True
        )
        
        for i, (msg_id, diff) in enumerate(sorted_diffs, 1):
            print(f"\n{i}. Message ID: {diff['msg_id']} - Confidence: {self._calculate_confidence(diff):.1f}%")
            
            if diff.get('action_correlated', False):
                print(f"   ✓ ACTION CORRELATED: Message appears consistently with each action performed")
                
            if diff['is_new']:
                print(f"   ✓ NEW MESSAGE: Not present in baseline, appeared {diff['action_count']} times")
            elif diff['freq_change_pct'] > 0:
                print(f"   ✓ FREQUENCY INCREASED: +{diff['freq_change_pct']:.1f}%")
            elif diff['freq_change_pct'] < 0:
                print(f"   ✓ FREQUENCY DECREASED: {diff['freq_change_pct']:.1f}%")
                
            if diff['new_payloads']:
                print(f"   ✓ NEW DATA PATTERNS: {len(diff['new_payloads'])} new patterns")
                for j, payload in enumerate(diff['new_payloads'][:3], 1):  # Show first 3
                    hex_payload = ' '.join([f"{b:02X}" for b in payload])
                    print(f"     {j}. {hex_payload}")
                if len(diff['new_payloads']) > 3:
                    print(f"     ... and {len(diff['new_payloads'])-3} more patterns")

    def _calculate_confidence(self, diff):
        """Calculate a confidence score for how likely a message is related to the action."""
        score = 0
        
        # New messages that weren't in baseline
        if diff['is_new']:
            score += 70
            
        # New data payloads
        if diff['new_payloads']:
            score += min(len(diff['new_payloads']) * 10, 20)
            
        # Frequency changes
        if abs(diff['freq_change_pct']) > 100:
            score += 10
        elif abs(diff['freq_change_pct']) > 50:
            score += 5
            
        # Repeated action correlation (if available)
        if 'action_correlated' in diff and diff['action_correlated']:
            score += 15  # Bonus for correlation with action timestamps
            
        return min(score, 100)  # Cap at 100%

    def plot_message_timeline(self, action_name, msg_ids=None):
        """
        Plot a timeline of when specific messages occurred.

        Args:
            action_name: Name of the capture to analyze
            msg_ids: List of message IDs to plot (if None, uses top differences)
        """
        if action_name not in self.captures:
            print(f"Capture '{action_name}' not found")
            return
            
        messages = self.captures[action_name]
        
        # Check if this is a multi-action capture by looking for metadata
        action_timestamps = []
        if messages and hasattr(messages[0], 'action_timestamps'):
            action_timestamps = messages[0].action_timestamps
            # Remove the metadata message from analysis
            messages = messages[1:]
        
        if not msg_ids:
            # If no message IDs specified, use the ones that changed most
            if "baseline" in self.captures:
                diffs = self.compare_captures(action_name)
                if diffs:
                    # Take the top 5 with highest confidence
                    sorted_diffs = sorted(
                        diffs.items(),
                        key=lambda x: self._calculate_confidence(x[1]),
                        reverse=True
                    )
                    msg_ids = [msg_id for msg_id, _ in sorted_diffs[:5]]
                else:
                    # If no differences, take the most frequent 5 messages
                    counter = defaultdict(int)
                    for msg in messages:
                        counter[msg.arbitration_id] += 1
                    msg_ids = [msg_id for msg_id, _ in sorted(counter.items(), key=lambda x: x[1], reverse=True)[:5]]
            else:
                # If no baseline, take the most frequent 5 messages
                counter = defaultdict(int)
                for msg in messages:
                    counter[msg.arbitration_id] += 1
                msg_ids = [msg_id for msg_id, _ in sorted(counter.items(), key=lambda x: x[1], reverse=True)[:5]]
        
        # Create plot
        plt.figure(figsize=(12, 6))
        
        # Each message ID gets its own y-position
        y_positions = {msg_id: i+1 for i, msg_id in enumerate(msg_ids)}
        
        # Plot each message as a point on the timeline
        for msg in messages:
            if msg.arbitration_id in y_positions:
                plt.plot(
                    msg.timestamp, 
                    y_positions[msg.arbitration_id], 
                    'o', 
                    markersize=4,
                    alpha=0.7
                )
        
        # Add vertical lines for action timestamps
        if action_timestamps:
            for i, timestamp in enumerate(action_timestamps):
                plt.axvline(x=timestamp, color='r', linestyle='--', alpha=0.7, 
                            label=f"Action {i+1}" if i == 0 else "")
            
            # Add a legend for the action lines
            plt.legend(loc='upper right')
        
        # Add labels and formatting
        plt.yticks(
            list(y_positions.values()),
            [f"{hex(msg_id)} ({msg_id})" for msg_id in y_positions.keys()]
        )
        plt.xlabel("Time (seconds)")
        plt.ylabel("Message ID")
        plt.title(f"Timeline of CAN Messages During '{action_name}' Action")
        plt.grid(True, axis='x', linestyle='--', alpha=0.7)
        
        # Save plot
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"{self.sessions_dir}/{timestamp}_{action_name}_timeline.png"
        plt.savefig(filename)
        print(f"Saved timeline plot to {filename}")
        
        try:
            plt.show()
        except Exception as e:
            print(f"Could not display plot (headless mode?): {e}")

    def capture_binary_toggle(self, action_name, toggle_count=5, state_duration=3):
        """
        Capture CAN messages during binary state toggles (on/off, lock/unlock, etc.).
        
        This method is optimized for identifying messages that toggle between two states.
        It prompts the user to alternate between two states multiple times.

        Args:
            action_name: Name of the action being toggled (e.g., 'door_lock', 'headlights')
            toggle_count: Number of times to toggle between states (default: 5)
            state_duration: Duration to hold each state in seconds (default: 3)

        Returns:
            List of captured CAN messages with toggle metadata
        """
        if not self.bus:
            if not self.connect():
                return []

        print(f"\n{'='*60}")
        print(f"BINARY TOGGLE CAPTURE: {action_name}")
        print(f"{'='*60}")
        print(f"This will capture {toggle_count} toggles between two states.")
        print(f"You'll hold each state for {state_duration} seconds.")
        print(f"Total capture time: ~{toggle_count * 2 * state_duration} seconds")
        print("\nInstructions:")
        print("1. Start with the action in the 'OFF' or initial state")
        print("2. When prompted, toggle to the 'ON' state")
        print("3. When prompted again, toggle back to the 'OFF' state")
        print("4. Repeat this pattern for all toggles")
        
        # Get user confirmation and initial state description
        initial_state = input(f"\nDescribe the initial state (e.g., 'doors unlocked', 'lights off'): ")
        toggled_state = input(f"Describe the toggled state (e.g., 'doors locked', 'lights on'): ")
        
        input(f"\nEnsure the {action_name} is in the '{initial_state}' state, then press Enter to begin...")
        
        # Countdown
        for i in range(3, 0, -1):
            print(f"Starting capture in {i}...")
            time.sleep(1)
        
        messages = []
        toggle_events = []  # Store when each toggle occurred and what state
        start_time = time.time()
        current_state = initial_state
        
        print(f"\nCapture started! Current state: '{current_state}'")
        
        try:
            for toggle_num in range(toggle_count * 2):  # Each toggle involves 2 state changes
                # Capture messages for the current state duration
                state_start_time = time.time()
                state_end_time = state_start_time + state_duration
                
                while time.time() < state_end_time:
                    msg = self.bus.recv(timeout=self.sample_rate)
                    if msg:
                        msg.timestamp = time.time() - start_time
                        messages.append(msg)
                
                # Prompt for state change (except after the last state)
                if toggle_num < (toggle_count * 2) - 1:
                    current_time = time.time() - start_time
                    
                    # Determine next state
                    if current_state == initial_state:
                        next_state = toggled_state
                    else:
                        next_state = initial_state
                    
                    # Record the toggle event
                    toggle_events.append({
                        'timestamp': current_time,
                        'from_state': current_state,
                        'to_state': next_state,
                        'toggle_number': (toggle_num // 2) + 1
                    })
                    
                    print(f"\n>>> NOW! Change from '{current_state}' to '{next_state}' <<<")
                    current_state = next_state
                    
                    # Brief pause to allow state change
                    time.sleep(0.5)
                else:
                    # Final state - just record the end
                    current_time = time.time() - start_time
                    print(f"\nCapture complete! Final state: '{current_state}'")
                    
        except KeyboardInterrupt:
            print("\nCapture interrupted by user.")
        
        print(f"\nCapture complete! Collected {len(messages)} messages across {len(toggle_events)} state changes.")
        
        # Add toggle metadata to the first message for later analysis
        if messages and toggle_events:
            meta_msg = type('Message', (), {})()
            meta_msg.timestamp = 0
            meta_msg.arbitration_id = 0
            meta_msg.dlc = 8
            meta_msg.data = bytes([0, 0, 0, 0, 0, 0, 0, 0])
            meta_msg.is_extended_id = False
            meta_msg.is_remote_frame = False
            meta_msg.is_error_frame = False
            meta_msg.toggle_events = toggle_events
            meta_msg.initial_state = initial_state
            meta_msg.toggled_state = toggled_state
            meta_msg.capture_type = 'binary_toggle'
            messages.insert(0, meta_msg)
        
        # Store the capture
        self.captures[action_name] = messages
        return messages

    def analyze_binary_toggles(self, action_name):
        """
        Analyze a binary toggle capture to identify messages that change between two states.
        
        Args:
            action_name: Name of the toggle capture to analyze
            
        Returns:
            Dictionary of message IDs and their binary toggle characteristics
        """
        if action_name not in self.captures:
            print(f"Toggle capture '{action_name}' not found")
            return None
            
        messages = self.captures[action_name].copy()
        
        # Extract toggle metadata
        if not messages or not hasattr(messages[0], 'toggle_events'):
            print(f"Capture '{action_name}' doesn't appear to be a binary toggle capture")
            return None
            
        toggle_events = messages[0].toggle_events
        initial_state = messages[0].initial_state
        toggled_state = messages[0].toggled_state
        
        # Remove metadata message
        messages = messages[1:]
        
        print(f"\nAnalyzing binary toggle capture: {action_name}")
        print(f"States: '{initial_state}' ↔ '{toggled_state}'")
        print(f"Toggle events: {len(toggle_events)}")
        
        # Group messages by ID and analyze payload patterns around toggle events
        toggle_analysis = {}
        
        # Group messages by arbitration ID
        messages_by_id = defaultdict(list)
        for msg in messages:
            messages_by_id[msg.arbitration_id].append(msg)
        
        # Analyze each message ID
        for msg_id, msg_list in messages_by_id.items():
            # Create timeline of payloads for this message ID
            payload_timeline = []
            for msg in msg_list:
                payload_timeline.append({
                    'timestamp': msg.timestamp,
                    'payload': bytes(msg.data)
                })
            
            # Sort by timestamp
            payload_timeline.sort(key=lambda x: x['timestamp'])
            
            # Analyze payload changes around toggle events
            toggle_correlations = []
            unique_payloads = set()
            payload_changes = []
            
            # Track payload changes
            for i in range(1, len(payload_timeline)):
                prev_payload = payload_timeline[i-1]['payload']
                curr_payload = payload_timeline[i]['payload']
                
                if prev_payload != curr_payload:
                    payload_changes.append({
                        'timestamp': payload_timeline[i]['timestamp'],
                        'from_payload': prev_payload,
                        'to_payload': curr_payload
                    })
                    unique_payloads.add(prev_payload)
                    unique_payloads.add(curr_payload)
            
            # Check correlation with toggle events
            correlated_changes = 0
            toggle_window = 2.0  # 2-second window around each toggle
            
            for toggle_event in toggle_events:
                toggle_time = toggle_event['timestamp']
                
                # Look for payload changes near this toggle
                for change in payload_changes:
                    if abs(change['timestamp'] - toggle_time) <= toggle_window:
                        correlated_changes += 1
                        toggle_correlations.append({
                            'toggle_event': toggle_event,
                            'payload_change': change,
                            'time_diff': change['timestamp'] - toggle_time
                        })
                        break
            
            # Calculate binary toggle characteristics
            is_binary_toggle = False
            binary_confidence = 0
            state_payloads = {}
            
            # Check if this message has exactly 2 unique payloads
            if len(unique_payloads) == 2:
                # Check if payload changes correlate well with state toggles
                correlation_ratio = correlated_changes / len(toggle_events) if toggle_events else 0
                
                if correlation_ratio >= 0.6:  # At least 60% of toggles have corresponding payload changes
                    is_binary_toggle = True
                    binary_confidence = min(correlation_ratio * 100, 100)
                    
                    # Try to map payloads to states
                    payloads = list(unique_payloads)
                    
                    # Use the first correlated change to determine mapping
                    if toggle_correlations:
                        first_correlation = toggle_correlations[0]
                        if first_correlation['toggle_event']['from_state'] == initial_state:
                            # Transition from initial to toggled state
                            state_payloads[initial_state] = first_correlation['payload_change']['from_payload']
                            state_payloads[toggled_state] = first_correlation['payload_change']['to_payload']
                        else:
                            # Transition from toggled to initial state
                            state_payloads[toggled_state] = first_correlation['payload_change']['from_payload']
                            state_payloads[initial_state] = first_correlation['payload_change']['to_payload']
            
            # Store analysis results
            if is_binary_toggle or correlated_changes > 0:
                toggle_analysis[msg_id] = {
                    'msg_id': hex(msg_id),
                    'is_binary_toggle': is_binary_toggle,
                    'binary_confidence': binary_confidence,
                    'unique_payloads': list(unique_payloads),
                    'payload_changes_count': len(payload_changes),
                    'correlated_changes': correlated_changes,
                    'correlation_ratio': correlated_changes / len(toggle_events) if toggle_events else 0,
                    'toggle_correlations': toggle_correlations,
                    'state_payloads': state_payloads,
                    'message_count': len(msg_list)
                }
        
        return toggle_analysis

    def print_binary_toggle_analysis(self, analysis):
        """
        Print the binary toggle analysis results in a readable format.
        
        Args:
            analysis: Dictionary from analyze_binary_toggles
        """
        if not analysis:
            print("No binary toggle patterns found")
            return
            
        print(f"\n{'=' * 80}")
        print(f"BINARY TOGGLE ANALYSIS RESULTS")
        print(f"{'=' * 80}")
        
        # Sort by binary confidence
        sorted_analysis = sorted(
            analysis.items(),
            key=lambda x: (x[1]['is_binary_toggle'], x[1]['binary_confidence']),
            reverse=True
        )
        
        binary_toggles = [item for item in sorted_analysis if item[1]['is_binary_toggle']]
        other_correlations = [item for item in sorted_analysis if not item[1]['is_binary_toggle']]
        
        if binary_toggles:
            print(f"\n🎯 CONFIRMED BINARY TOGGLE MESSAGES ({len(binary_toggles)} found):")
            print("=" * 60)
            
            for i, (msg_id, data) in enumerate(binary_toggles, 1):
                print(f"\n{i}. Message ID: {data['msg_id']} - Confidence: {data['binary_confidence']:.1f}%")
                print(f"   ✓ BINARY TOGGLE DETECTED: Changes between exactly 2 payloads")
                print(f"   ✓ Correlation: {data['correlated_changes']}/{len(data['toggle_correlations'])} toggles matched")
                
                if data['state_payloads']:
                    print(f"   ✓ STATE MAPPING:")
                    for state, payload in data['state_payloads'].items():
                        hex_payload = ' '.join([f"{b:02X}" for b in payload])
                        print(f"     '{state}': {hex_payload}")
                else:
                    print(f"   ✓ PAYLOADS:")
                    for j, payload in enumerate(data['unique_payloads'], 1):
                        hex_payload = ' '.join([f"{b:02X}" for b in payload])
                        print(f"     Payload {j}: {hex_payload}")
        
        if other_correlations:
            print(f"\n📊 OTHER CORRELATED MESSAGES ({len(other_correlations)} found):")
            print("=" * 60)
            
            for i, (msg_id, data) in enumerate(other_correlations, 1):
                print(f"\n{i}. Message ID: {data['msg_id']} - Correlation: {data['correlation_ratio']:.1%}")
                print(f"   • {len(data['unique_payloads'])} unique payloads")
                print(f"   • {data['correlated_changes']} payload changes near toggle events")
                
                if len(data['unique_payloads']) <= 3:  # Show payloads if not too many
                    for j, payload in enumerate(data['unique_payloads'], 1):
                        hex_payload = ' '.join([f"{b:02X}" for b in payload])
                        print(f"     Payload {j}: {hex_payload}")

def interactive_session(analyzer):
    """Run an interactive CAN bus analysis session."""
    

    # Display welcome message
    print("\n" + "=" * 80)
    print("CAN BUS ACTION ANALYZER".center(80))
    print("For identifying messages related to vehicle actions".center(80))
    print("=" * 80 + "\n")

    # Menu system
    while True:
        print("\nMAIN MENU:")
        print("1. Establish baseline (capture normal traffic)")
        print("2. Capture traffic during specific action")
        print("3. Analyze differences between captures")
        print("4. Save/load captures")
        print("5. Visualize message timeline")
        print("6. Capture binary toggle action")
        print("7. Analyze binary toggle capture")
        print("0. Exit")
        
        choice = input("\nEnter your choice (0-7): ")
        
        if choice == '1':
            # Baseline capture
            duration = float(input("Enter capture duration in seconds [10]: ") or "10")
            print("\nWe'll capture baseline traffic with no actions performed.")
            print("Keep the vehicle idle during this capture.")
            input("Press Enter when ready to begin baseline capture...")
            analyzer.capture_messages(duration, "baseline")
            analyzer.save_capture("baseline")
            
        elif choice == '2':
            # Action capture
            action_name = input("Enter a name for this action (e.g., 'lock_doors', 'lights_on'): ")
            duration = float(input("Enter capture duration in seconds [10]: ") or "10")
            
            # Ask if they want to repeat the action multiple times
            repeat = input("Would you like to perform this action multiple times for better confidence? (y/n) [n]: ").lower()
            
            if repeat == 'y':
                repeat_count = int(input("Enter number of times to perform the action [3]: ") or "3")
                repeat_interval = float(input("Enter seconds between each action [5]: ") or "5")
                
                print(f"\nWe'll capture traffic during the '{action_name}' action, repeated {repeat_count} times.")
                print("Instructions:")
                print("1. Get ready to perform the action")
                print(f"2. When prompted with 'NOW!', perform the action immediately")
                print("3. Wait for the next prompt before performing the action again")
                print(f"4. You'll be asked to repeat the action {repeat_count} times in total")
                
                input("Press Enter when ready...")
                analyzer.capture_messages(duration, action_name, repeat_count=repeat_count, repeat_interval=repeat_interval)
            else:
                print(f"\nWe'll capture traffic during the '{action_name}' action.")
                print("Instructions:")
                print("1. Get ready to perform the action")
                print("2. When prompted with 'NOW!', perform the action immediately")
                print("3. Remain still after performing the action until capture completes")
                
                input("Press Enter when ready...")
                analyzer.capture_messages(duration, action_name)
                
            analyzer.save_capture(action_name)
            
        elif choice == '3':
            # Analyze differences
            if len(analyzer.captures) < 2:
                print("You need at least 2 captures (baseline and an action) to analyze differences.")
                continue
                
            print("\nAvailable captures:")
            for i, name in enumerate(analyzer.captures.keys(), 1):
                print(f"{i}. {name} ({len(analyzer.captures[name])} messages)")
                
            action_name = input("\nEnter the name of the action capture to analyze: ")
            baseline_name = input("Enter the name of the baseline capture [baseline]: ") or "baseline"
            
            if action_name in analyzer.captures and baseline_name in analyzer.captures:
                differences = analyzer.compare_captures(action_name, baseline_name)
                analyzer.print_differences(differences)
                
                # Ask if user wants to visualize the results
                if input("\nVisualize these results on a timeline? (y/n): ").lower() == 'y':
                    # Extract message IDs from differences
                    if differences:
                        msg_ids = list(differences.keys())
                        analyzer.plot_message_timeline(action_name, msg_ids)
            else:
                print("One or both of the specified captures don't exist.")
                
        elif choice == '4':
            # Save/load submenu
            print("\nSAVE/LOAD MENU:")
            print("1. List available saved captures")
            print("2. Load a capture from file")
            print("3. Return to main menu")
            
            subchoice = input("\nEnter your choice (1-3): ")
            
            if subchoice == '1':
                if not os.path.exists(analyzer.sessions_dir):
                    print("No saved captures found.")
                    continue
                    
                files = os.listdir(analyzer.sessions_dir)
                json_files = [f for f in files if f.endswith('.json')]
                
                if not json_files:
                    print("No saved captures found.")
                    continue
                    
                print("\nSaved captures:")
                for i, filename in enumerate(json_files, 1):
                    print(f"{i}. {filename}")
                    
            elif subchoice == '2':
                if not os.path.exists(analyzer.sessions_dir):
                    print("No saved captures found.")
                    continue
                    
                files = os.listdir(analyzer.sessions_dir)
                json_files = [f for f in files if f.endswith('.json')]
                
                if not json_files:
                    print("No saved captures found.")
                    continue
                    
                print("\nAvailable captures to load:")
                for i, filename in enumerate(json_files, 1):
                    print(f"{i}. {filename}")
                    
                file_idx = input("\nEnter the number of the file to load: ")
                try:
                    idx = int(file_idx) - 1
                    if 0 <= idx < len(json_files):
                        full_path = os.path.join(analyzer.sessions_dir, json_files[idx])
                        analyzer.load_capture(full_path)
                    else:
                        print("Invalid selection.")
                except ValueError:
                    print("Invalid input.")
                    
        elif choice == '5':
            # Visualize timeline
            if not analyzer.captures:
                print("No captures available to visualize.")
                continue
                
            print("\nAvailable captures:")
            for i, name in enumerate(analyzer.captures.keys(), 1):
                print(f"{i}. {name} ({len(analyzer.captures[name])} messages)")
                
            action_name = input("\nEnter the name of the capture to visualize: ")
            
            if action_name in analyzer.captures:
                # Ask if they want to specify message IDs
                if input("Specify message IDs to plot? (y/n): ").lower() == 'y':
                    ids_input = input("Enter message IDs (decimal) separated by commas: ")
                    try:
                        msg_ids = [int(x.strip()) for x in ids_input.split(',')]
                        analyzer.plot_message_timeline(action_name, msg_ids)
                    except ValueError:
                        print("Invalid input. Using top messages instead.")
                        analyzer.plot_message_timeline(action_name)
                else:
                    analyzer.plot_message_timeline(action_name)
            else:
                print("Specified capture doesn't exist.")
                
        elif choice == '6':
            # Binary toggle capture
            action_name = input("Enter a name for this toggle action (e.g., 'door_lock_toggle'): ")
            toggle_count = int(input("Enter number of toggles to capture [5]: ") or "5")
            state_duration = float(input("Enter duration for each state in seconds [3]: ") or "3")
            
            analyzer.capture_binary_toggle(action_name, toggle_count=toggle_count, state_duration=state_duration)
            analyzer.save_capture(action_name)
            
            # Automatically analyze the binary toggle capture
            if input("\nAnalyze this binary toggle capture now? (y/n) [y]: ").lower() != 'n':
                analysis = analyzer.analyze_binary_toggles(action_name)
                if analysis:
                    analyzer.print_binary_toggle_analysis(analysis)
                    
                    # Ask if user wants to visualize the binary toggle results
                    if input("\nVisualize binary toggle timeline? (y/n): ").lower() == 'y':
                        # Get the confirmed binary toggle message IDs
                        binary_msg_ids = [msg_id for msg_id, data in analysis.items() if data['is_binary_toggle']]
                        if binary_msg_ids:
                            analyzer.plot_message_timeline(action_name, binary_msg_ids)
                        else:
                            analyzer.plot_message_timeline(action_name)
            
        elif choice == '7':
            # Analyze binary toggle capture
            if not analyzer.captures:
                print("No captures available to analyze.")
                continue
                
            print("\nAvailable captures:")
            for i, name in enumerate(analyzer.captures.keys(), 1):
                capture_type = "Binary Toggle" if (analyzer.captures[name] and 
                                                 hasattr(analyzer.captures[name][0], 'capture_type') and 
                                                 analyzer.captures[name][0].capture_type == 'binary_toggle') else "Regular"
                print(f"{i}. {name} ({len(analyzer.captures[name])} messages) - {capture_type}")
                
            action_name = input("\nEnter the name of the binary toggle capture to analyze: ")
            
            if action_name in analyzer.captures:
                analysis = analyzer.analyze_binary_toggles(action_name)
                if analysis:
                    analyzer.print_binary_toggle_analysis(analysis)
                    
                    # Ask if user wants to visualize the results
                    if input("\nVisualize binary toggle timeline? (y/n): ").lower() == 'y':
                        # Get the confirmed binary toggle message IDs
                        binary_msg_ids = [msg_id for msg_id, data in analysis.items() if data['is_binary_toggle']]
                        if binary_msg_ids:
                            analyzer.plot_message_timeline(action_name, binary_msg_ids)
                        else:
                            analyzer.plot_message_timeline(action_name)
            else:
                print("Specified capture doesn't exist.")
                
        elif choice == '0':
            # Exit
            print("\nExiting CAN bus analyzer. Goodbye!")
            analyzer.disconnect()
            break
            
        else:
            print("Invalid choice. Please try again.")


def main():
    """Main entry point for the script."""
    parser = argparse.ArgumentParser(
        description="CAN Bus Analyzer for Vehicle Action Detection",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        'can_interface',
        help="CAN interface to use (e.g., can0, slcan0)"
    )
    
    args = parser.parse_args()
    
    # Print welcome message
    print("\n" + "=" * 80)
    print("CAN BUS ACTION ANALYZER".center(80))
    print("For identifying messages related to vehicle actions".center(80))
    print("=" * 80 + "\n")
    
    # Create analyzer instance
    analyzer = CANActionAnalyzer(can_interface=args.can_interface)
    
    # Connect to CAN bus
    if not analyzer.connect():
        sys.exit(1)
    
    # Run interactive session
    interactive_session(analyzer)
    
    # Cleanup on exit
    analyzer.disconnect()


if __name__ == "__main__":
    main()
