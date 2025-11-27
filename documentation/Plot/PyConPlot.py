#!/usr/bin/env python3

"""Wrapper for matplotlib to create various plots (multi values, multi panes)

see README.md

"""

import argparse
import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
from collections import defaultdict
from typing import Dict, List, Tuple, Any, Optional, Union
import numpy as np
import shlex

def get_available_palettes() -> List[str]:
    """Get list of available matplotlib color palettes/colormaps"""
    return sorted(plt.colormaps())

def get_legend_positions() -> List[str]:
    """Get list of available matplotlib legend positions"""
    return [
        'best', 'upper right', 'upper left', 'lower left', 'lower right',
        'right', 'center left', 'center right', 'lower center',
        'upper center', 'center'
    ]

def str2bool(v: Union[str, bool]) -> bool:
    """Convert string to boolean"""
    if isinstance(v, bool):
        return v
    if v.lower() in ('yes', 'true', 't', 'y', '1'):
        return True
    elif v.lower() in ('no', 'false', 'f', 'n', '0'):
        return False
    else:
        raise argparse.ArgumentTypeError('Boolean value expected.')

def parse_plot_options(option_string: str) -> Dict[str, Any]:
    """Parse plot-specific options from @New plot line"""
    options: Dict[str, Any] = {}
    if ':' not in option_string:
        return options
    _, opts_part = option_string.split(':', 1)
    try:
        tokens = shlex.split(opts_part)
    except ValueError:
        tokens = opts_part.split()
    for token in tokens:
        if '=' in token:
            key, value = token.split('=', 1)
            key = key.strip()
            value = value.strip()
            if key in ['logx', 'logy', 'normalize', 'grid', 'grid_major', 'grid_minor']:
                try:
                    options[key] = str2bool(value)
                except:
                    options[key] = value.lower() in ('true', '1', 'yes')
            elif key in ['miny', 'maxy', 'linewidth']:
                try:
                    options[key] = float(value)
                except:
                    pass
            elif key in ['width', 'height']:
                try:
                    options[key] = int(value)
                except:
                    pass
            else:
                options[key] = value  # Handle title here
    return options

def setup_theme(inverse: bool) -> None:
    """Setup the plot theme (normal or inverse)"""
    if inverse:
        plt.style.use('dark_background')
    else:
        plt.style.use('default')

def setup_grid(ax: plt.Axes, args: argparse.Namespace, plot_options: Optional[Dict[str, Any]] = None) -> None:
    """Setup grid for the axes"""
    if plot_options is None:
        plot_options = {}
    grid_enabled = plot_options.get('grid', getattr(args, 'grid', False))
    grid_major = plot_options.get('grid_major', getattr(args, 'grid_major', True))
    grid_minor = plot_options.get('grid_minor', getattr(args, 'grid_minor', False))
    if grid_enabled or grid_major or grid_minor:
        if grid_minor:
            ax.minorticks_on()
        if grid_major:
            ax.grid(True, which='major', alpha=0.7, linewidth=0.8)
        if grid_minor:
            ax.grid(True, which='minor', alpha=0.3, linewidth=0.5, linestyle=':')

def normalize_data(data: Dict[str, Dict[str, List[float]]]) -> Dict[str, Dict[str, List[float]]]:
    """Normalize data to range [0, 1]"""
    normalized_data: Dict[str, Dict[str, List[float]]] = {}
    for name, values in data.items():
        x_vals = np.array(values['x'])
        y_vals = np.array(values['y'])
        if len(x_vals) > 1:
            x_min, x_max = x_vals.min(), x_vals.max()
            x_norm = (x_vals - x_min) / (x_max - x_min) if x_max != x_min else x_vals
        else:
            x_norm = x_vals
        if len(y_vals) > 1:
            y_min, y_max = y_vals.min(), y_vals.max()
            y_norm = (y_vals - y_min) / (y_max - y_min) if y_max != y_min else y_vals
        else:
            y_norm = y_vals
        normalized_data[name] = {'x': x_norm.tolist(), 'y': y_norm.tolist()}
    return normalized_data

def parse_data_file(filename: str) -> List[Tuple[Dict[str, Dict[str, List[float]]], Dict[str, Any]]]:
    """Parse the input txt file and return list of plot groups with their options"""
    plots: List[Tuple[Dict[str, Dict[str, List[float]]], Dict[str, Any]]] = []
    current_plot_data: Dict[str, Dict[str, List[float]]] = defaultdict(lambda: {'x': [], 'y': []})
    current_plot_options: Dict[str, Any] = {}
    current_name: Optional[str] = None
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            if line.startswith('@New plot') or line.startswith('@new plot'):
                if current_plot_data:
                    plots.append((dict(current_plot_data), current_plot_options))
                current_plot_data = defaultdict(lambda: {'x': [], 'y': []})
                current_plot_options = parse_plot_options(line)
                continue
            if line.startswith('#'):
                current_name = line[1:].strip()
            else:
                if current_name is not None:
                    try:
                        x, y = map(float, line.split())
                        current_plot_data[current_name]['x'].append(x)
                        current_plot_data[current_name]['y'].append(y)
                    except ValueError:
                        print(f"Warning: Could not parse line: {line}")
                        continue
    if current_plot_data:
        plots.append((dict(current_plot_data), current_plot_options))
    return plots if plots else [(defaultdict(lambda: {'x': [], 'y': []}), {})]

def create_plots(data_plots: List[Tuple[Dict[str, Dict[str, List[float]]], Dict[str, Any]]], args: argparse.Namespace) -> None:
    """Create and save the plots"""
    n_plots = len(data_plots)
    setup_theme(args.inverse)
    if n_plots == 1:
        fig, ax = plt.subplots(figsize=(args.width/100, args.height/100))
        axes = [ax]
    else:
        cols = int(np.ceil(np.sqrt(n_plots)))
        rows = int(np.ceil(n_plots / cols))
        fig, axes = plt.subplots(rows, cols, figsize=(args.width/100 * cols, args.height/100 * rows))
        axes = axes.flatten() if hasattr(axes, 'flatten') else axes
    for plot_idx, (data, plot_options) in enumerate(data_plots):
        if plot_idx >= len(axes):
            break
        ax = axes[plot_idx] if n_plots > 1 else axes[0]
        effective_options = vars(args).copy()
        effective_options.update(plot_options)
        palette = effective_options.get('palette', args.palette)
        try:
            cmap = plt.get_cmap(palette)
        except ValueError:
            print(f"Warning: Palette '{palette}' not found. Using default 'Set1'.")
            cmap = plt.get_cmap('Set1')
        if effective_options.get('normalize', False):
            data = normalize_data(data)
        color_idx = 0
        for name, values in data.items():
            if not values['x'] or not values['y']:
                continue
            color = cmap(color_idx / max(1, len(data) - 1)) if len(data) > 1 else cmap(0)
            linewidth = effective_options.get('linewidth', args.linewidth)
            ax.plot(values['x'], values['y'], label=name, color=color, linewidth=linewidth)
            color_idx += 1
        if effective_options.get('logx', False):
            ax.set_xscale('log')
        if effective_options.get('logy', False):
            ax.set_yscale('log')
        setup_grid(ax, args, plot_options)
        labelx = effective_options.get('labelx', args.labelx)
        labely = effective_options.get('labely', args.labely)
        ax.set_xlabel(labelx)
        ax.set_ylabel(labely)
        miny = effective_options.get('miny', args.miny)
        maxy = effective_options.get('maxy', args.maxy)
        if miny is not None or maxy is not None:
            current_ylim = ax.get_ylim()
            ymin = miny if miny is not None else current_ylim[0]
            ymax = maxy if maxy is not None else current_ylim[1]
            ax.set_ylim(ymin, ymax)
        # Add legend if there are multiple series
        if len(data) > 1:
            legendpos = effective_options.get('legendpos', args.legendpos)
            ax.legend(loc=legendpos)
        # Add title if specified in options
        title = effective_options.get('title', None)
        if title is not None:
            ax.set_title(title)
        elif n_plots > 1:
            ax.set_title(f'Plot {plot_idx + 1}')
    if n_plots > 1:
        for i in range(n_plots, len(axes)):
            axes[i].set_visible(False)
    plt.tight_layout()
    try:
        plt.savefig(args.outfile, dpi=150, bbox_inches='tight')
        print(f"Plot saved to: {args.outfile}")
    except Exception as e:
        print(f"Error saving plot: {e}")
    plt.close()

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Plot multiple groups of 2D data from a text file with grid and theme support",
        epilog=f"""
Available palettes: {', '.join(get_available_palettes()[:20])}... (and many more)
Legend positions: {', '.join(get_legend_positions())}
Text file format:
#group1
x1 y1
x2 y2
#group2
x3 y3
x4 y4

@New plot: labelx="Frequency (Hz)" labely="Magnitude (dB)" title="Frequency Response" logx=true logy=false grid=true
#group3
x5 y5

Plot-specific options (use in @New plot lines):
- labelx, labely: axis labels
- title: plot title
- logx, logy: logarithmic scales (true/false)
- normalize: normalize data (true/false)
- miny, maxy: y-axis limits (numbers)
- linewidth: line width (number)
- palette: color palette name
- legendpos: legend position
- grid: enable grid (true/false)
- grid_major: enable major grid (true/false)
- grid_minor: enable minor grid (true/false)
""",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument('-f', '--infile', required=True, help='Input text file with data')
    parser.add_argument('-o', '--outfile', default='plot.svg', help='Output file (can be png, svg, pdf, etc.) (default: plot.svg)')
    parser.add_argument('-w', '--width', type=int, default=1200, help='Width of the plot in pixels (default: 1200)')
    parser.add_argument('--height', type=int, default=800, help='Height of the plot in pixels (default: 800)')
    parser.add_argument('-n', '--normalize', action='store_true', help='Normalize values to [0,1] range')
    parser.add_argument('--labelx', default='x', help='Label for x-axis (default: "x")')
    parser.add_argument('--labely', default='y', help='Label for y-axis (default: "y")')
    parser.add_argument('--palette', default='Set1', help='Color palette name from matplotlib (default: "Set1")')
    parser.add_argument('--legendpos', default='best', choices=get_legend_positions(), help='Legend position (default: "best")')
    parser.add_argument('--linewidth', type=float, default=1.0, help='Line width for plotting (default: 1.0)')
    parser.add_argument('--miny', type=float, help='Minimum y-axis value (default: auto)')
    parser.add_argument('--maxy', type=float, help='Maximum y-axis value (default: auto)')
    parser.add_argument('--logx', action='store_true', help='Use logarithmic scale for x-axis')
    parser.add_argument('--logy', action='store_true', help='Use logarithmic scale for y-axis')
    parser.add_argument('--grid', action='store_true', help='Enable grid (same as --grid-major)')
    parser.add_argument('--grid-major', action='store_true', default=True, help='Enable major grid lines (default: True)')
    parser.add_argument('--grid-minor', action='store_true', help='Enable minor grid lines (default: False)')
    parser.add_argument('--no-grid', action='store_true', help='Disable all grid lines')
    parser.add_argument('--inverse', action='store_true', help='Use dark theme (white on black)')
    args = parser.parse_args()
    if args.no_grid:
        args.grid_major = False
        args.grid_minor = False
    elif args.grid:
        args.grid_major = True
    try:
        data_plots = parse_data_file(args.infile)
        if not data_plots or not any(data[0] for data in data_plots):
            print("No data found in input file")
            return
        create_plots(data_plots, args)
    except FileNotFoundError:
        print(f"Error: File '{args.infile}' not found")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()
