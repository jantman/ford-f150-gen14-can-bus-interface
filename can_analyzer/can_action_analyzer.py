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
                
            messages.append(msg)
            
        self.captures[action_name] = messages
        
        # Detect if this is a multi-action capture
        has_timestamps = any(hasattr(msg, 'action_timestamps') for msg in messages)
        if has_timestamps:
            action_count = len(messages[0].action_timestamps) if hasattr(messages[0], 'action_timestamps') else 1
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
        print("0. Exit")
        
        choice = input("\nEnter your choice (0-5): ")
        
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
