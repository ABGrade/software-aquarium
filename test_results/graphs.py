import pandas as pd
import matplotlib.pyplot as plt
import os

# Настройка стиля для академического вида и ЧБ печати
plt.style.use('seaborn-v0_8-whitegrid')
plt.rcParams.update({
    'font.family': 'serif',
    'font.size': 12,
    'axes.labelsize': 12,
    'axes.titlesize': 14,
    'xtick.labelsize': 10,
    'ytick.labelsize': 10,
    'legend.fontsize': 10,
    'figure.titlesize': 16,
    'text.color': 'black',
    'axes.labelcolor': 'black',
    'xtick.color': 'black',
    'ytick.color': 'black'
})

def plot_fish_vs_time():
    filename = 'fish_vs_time.csv'
    if not os.path.exists(filename):
        print(f"Файл {filename} не найден!")
        return

    df = pd.read_csv(filename)
    
    plt.figure(figsize=(8, 5))
    # Используем черный цвет для максимального контраста
    plt.plot(df['fish_count'], df['avg_ms'], marker='o', linestyle='-', color='black', linewidth=2, label='Время кадра')
    
    plt.title('Зависимость времени рендеринга от количества рыб')
    plt.xlabel('Количество рыб (шт)')
    plt.ylabel('Время кадра (мс)')
    
    plt.xticks(df['fish_count'])
    plt.ylim(bottom=0, top=df['avg_ms'].max() * 1.2)
    
    plt.grid(True, linestyle='--', alpha=0.5, color='gray')
    plt.tight_layout()
    plt.savefig('fish_vs_time_plot.png', dpi=300)
    print("График fish_vs_time_plot.png сохранен.")
    plt.close()

def plot_segments_vs_time():
    filename = 'segments_vs_time.csv'
    if not os.path.exists(filename):
        print(f"Файл {filename} не найден!")
        return

    df = pd.read_csv(filename)
    
    plt.figure(figsize=(8, 5))
    # Используем темно-серый цвет и квадратные маркеры
    plt.plot(df['segments'], df['avg_ms'], marker='s', linestyle='-', color='#333333', linewidth=2, label='Время кадра')
    
    plt.title('Зависимость времени рендеринга от детализации')
    plt.xlabel('Количество сегментов')
    plt.ylabel('Время кадра (мс)')
    
    for x, y in zip(df['segments'], df['avg_ms']):
        plt.text(x, y + 2, f"{y:.1f}", ha='center', va='bottom', fontsize=9)

    plt.ylim(200, 260)
    plt.grid(True, linestyle='--', alpha=0.5, color='gray')
    plt.tight_layout()
    plt.savefig('segments_vs_time_plot.png', dpi=300)
    print("График segments_vs_time_plot.png сохранен.")
    plt.close()

def plot_camera_vs_time():
    filename = 'camera_vs_time.csv'
    if not os.path.exists(filename):
        print(f"Файл {filename} не найден!")
        return

    df = pd.read_csv(filename)
    
    labels = ['Стандартный', 'Симметричный', 'Вид сверху']
    if len(df) != len(labels):
        labels = [f"Scen. {i+1}" for i in range(len(df))]

    plt.figure(figsize=(8, 5))
    
    # Настройка штриховки (hatching)
    # '/' - диагональ, '.' - точки, 'x' - сетка
    patterns = ['///', '...', 'xxx'] 
    
    bars = plt.bar(labels, df['avg_ms'], 
                   color='white',      # Белая заливка, чтобы штриховка была видна
                   edgecolor='black',  # Черный контур
                   width=0.5)
    
    # Применяем узор к каждому столбцу отдельно
    for bar, pattern in zip(bars, patterns):
        bar.set_hatch(pattern)
    
    plt.title('Влияние ракурса камеры на производительность')
    plt.ylabel('Время кадра (мс)')
    plt.xlabel('Сценарий обзора')
    
    for bar in bars:
        height = bar.get_height()
        plt.text(bar.get_x() + bar.get_width()/2., height,
                 f'{height:.1f} мс',
                 ha='center', va='bottom', color='black')

    plt.ylim(0, df['avg_ms'].max() * 1.2)
    plt.grid(axis='y', linestyle='--', alpha=0.5, color='gray')
    plt.tight_layout()
    plt.savefig('camera_vs_time_bar.png', dpi=300)
    print("График camera_vs_time_bar.png сохранен.")
    plt.close()

if __name__ == "__main__":
    plot_fish_vs_time()
    plot_segments_vs_time()
    plot_camera_vs_time()