import pandas as pd
import numpy as np
from datetime import datetime
import matplotlib.pyplot as plt
import seaborn as sns
from scipy import stats

def analyze_tuning_data(file_path):

    df = pd.read_csv(r'C:\Users\lukef\OneDrive\Documents\York St John - Computer Science\Year 3\Internet Of Things\guitar_tuner_data\tuning_data.csv')
    df['Timestamp'] = pd.to_datetime(df['Timestamp'])
    
    # 1. Basic Statistics
    print("=== BASIC STATISTICS ===")
    print("\nFrequency Statistics:")
    print(df['Frequency (Hz)'].describe())
    
    # 2. Tuning Status Analysis
    status_counts = df['Tuning Status'].value_counts()
    status_percentages = (status_counts / len(df) * 100).round(2)
    
    print("\n=== TUNING STATUS ANALYSIS ===")
    print("\nCounts and Percentages:")
    for status, count in status_counts.items():
        print(f"{status}: {count} ({status_percentages[status]}%)")
    
    # 3. Frequency Range Analysis
    print("\n=== FREQUENCY RANGE ANALYSIS ===")
    for status in df['Tuning Status'].unique():
        status_data = df[df['Tuning Status'] == status]
        print(f"\n{status}:")
        print(f"Mean: {status_data['Frequency (Hz)'].mean():.2f} Hz")
        print(f"Std Dev: {status_data['Frequency (Hz)'].std():.2f} Hz")
        print(f"Range: {status_data['Frequency (Hz)'].min():.2f} - "
              f"{status_data['Frequency (Hz)'].max():.2f} Hz")
    
    # 4. Time-based Analysis
    df['Hour'] = df['Timestamp'].dt.hour
    hourly_counts = df.groupby('Hour').size()
    
    print("\n=== TEMPORAL ANALYSIS ===")
    print("\nMeasurements by hour of day:")
    print(hourly_counts)
    
    # 5. Statistical Tests
    print("\n=== STATISTICAL ANALYSIS ===")
    
    # ANOVA test between frequency groups
    frequencies_by_status = [group['Frequency (Hz)'].values 
                           for name, group in df.groupby('Tuning Status')]
    f_stat, p_value = stats.f_oneway(*frequencies_by_status)
    print(f"\nOne-way ANOVA test for frequency differences between tuning statuses:")
    print(f"F-statistic: {f_stat:.4f}")
    print(f"p-value: {p_value:.4f}")
    
    # 6. Transition Analysis
    if len(df) > 1:
        df['Next_Status'] = df['Tuning Status'].shift(-1)
        transitions = pd.crosstab(df['Tuning Status'], df['Next_Status'])
        
        print("\n=== TRANSITION ANALYSIS ===")
        print("\nStatus Transition Matrix:")
        print(transitions)
    
    # 7. Visualizations
    plt.style.use('seaborn')
    
    # Frequency Distribution by Status
    plt.figure(figsize=(12, 6))
    sns.violinplot(x='Tuning Status', y='Frequency (Hz)', data=df)
    plt.title('Frequency Distribution by Tuning Status')
    plt.xticks(rotation=45)
    plt.tight_layout()
    plt.savefig(r'frequency_distribution.png')  # Note the 'r' prefix
    
    # Time Series Plot
    plt.figure(figsize=(15, 6))
    for status in df['Tuning Status'].unique():
        status_data = df[df['Tuning Status'] == status]
        plt.scatter(status_data['Timestamp'], 
                   status_data['Frequency (Hz)'],
                   label=status, alpha=0.6)
    plt.title('Frequency Measurements Over Time')
    plt.xlabel('Timestamp')
    plt.ylabel('Frequency (Hz)')
    plt.legend()
    plt.xticks(rotation=45)
    plt.tight_layout()
    plt.savefig(r'time_series.png')  # Note the 'r' prefix
    
    # 8. Advanced Metrics
    print("\n=== ADVANCED METRICS ===")
    
    # Calculate drift (if measurements are ordered)
    df['Freq_Change'] = df['Frequency (Hz)'].diff()
    print("\nFrequency Drift Statistics:")
    print(df['Freq_Change'].describe())
    
    # Calculate stability score (inverse of variance)
    stability_score = 1 / (df['Frequency (Hz)'].std() + 1e-6)
    print(f"\nTuning Stability Score: {stability_score:.4f}")
    
    # Calculate success rate
    success_rate = len(df[df['Tuning Status'] == 'In Tune']) / len(df)
    print(f"Overall Success Rate: {success_rate:.2%}")
    
    return df

if __name__ == "__main__":
    # Run the analysis
    df = analyze_tuning_data('tuning_data.csv')
    
    # Additional custom analysis can be performed here using the returned dataframe
    # For example:
    consecutive_in_tune = df['Tuning Status'].eq('In Tune').astype(int).groupby(
        df['Tuning Status'].ne('In Tune').cumsum()).sum()
    
    print("\n=== ADDITIONAL METRICS ===")
    print(f"Longest streak of in-tune measurements: {consecutive_in_tune.max()}")