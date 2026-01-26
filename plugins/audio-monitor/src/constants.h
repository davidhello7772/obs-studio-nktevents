/*
 * Audio Monitor Plugin - Constants
 * Centralized configuration values and magic numbers
 */

#pragma once

#include <QColor>

namespace AudioMonitorConstants {

// ============================================================================
// Widget Dimensions
// ============================================================================

// Card dimensions
constexpr int CARD_WIDTH_DEFAULT = 155;
constexpr int CARD_WIDTH_MIN = 120;
constexpr int CARD_WIDTH_MAX = 250;
constexpr int CARD_WIDTH_STEP = 5;
constexpr int CARD_MIN_HEIGHT = 340;
constexpr int METER_MIN_WIDTH = 50;
constexpr int METER_MIN_HEIGHT = 160;

// Button sizes
constexpr int BUTTON_SIZE = 28;
constexpr int ICON_SIZE = 16;

// Badge dimensions
constexpr int BADGE_HEIGHT = 18;
constexpr int BADGE_CONTAINER_HEIGHT = 20;
constexpr int NAME_LABEL_HEIGHT = 32;

// Filter list dimensions
constexpr int MAX_VISIBLE_FILTERS = 4;
constexpr int FILTER_LIST_FIXED_HEIGHT = 90;
constexpr int FILTER_ITEM_HEIGHT = 20;

// ============================================================================
// Audio Level Thresholds (in dBFS)
// ============================================================================

// Volume history for averaging (samples at ~10Hz, ~10 seconds)
constexpr size_t VOLUME_HISTORY_SIZE = 100;
constexpr double SILENCE_LEVEL_DB = -60.0;

// Meter thresholds for Mix sources
constexpr double MIX_NOMINAL_LEVEL_DB = -28.0;
constexpr double MIX_WARNING_LEVEL_DB = -20.0;
constexpr double MIX_ERROR_LEVEL_DB = -3.0;

// Meter thresholds for Translated sources
constexpr double TRANSLATED_NOMINAL_LEVEL_DB = -9.0;
constexpr double TRANSLATED_WARNING_LEVEL_DB = -6.0;
constexpr double TRANSLATED_ERROR_LEVEL_DB = -3.0;

// Meter thresholds for Normal/Reference sources (no blue zone)
constexpr double NORMAL_NOMINAL_LEVEL_DB = -60.0;  // Same as minimum = no blue zone
constexpr double NORMAL_WARNING_LEVEL_DB = -6.0;
constexpr double NORMAL_ERROR_LEVEL_DB = -3.0;

// Configurable threshold ranges (for header controls)
constexpr double NOMINAL_THRESHOLD_MIN = -30.0;
constexpr double NOMINAL_THRESHOLD_MAX = -3.0;
constexpr double WARNING_THRESHOLD_MIN = -20.0;
constexpr double WARNING_THRESHOLD_MAX = -1.0;
constexpr double ERROR_THRESHOLD_MIN = -10.0;
constexpr double ERROR_THRESHOLD_MAX = 0.0;
constexpr double THRESHOLD_STEP = 1.0;

// Minimum gaps between thresholds to prevent overlap
constexpr double NOMINAL_WARNING_GAP = 3.0;  // nominal must be at least 3dB below warning
constexpr double WARNING_ERROR_GAP = 2.0;    // warning must be at least 2dB below error

// ============================================================================
// Volume Monitoring Configuration
// ============================================================================

// Threshold for HIGH/LOW volume warnings (dB difference from reference)
constexpr double WARNING_THRESHOLD_DB = 10.0;

// Reference must be at least this loud to trigger LOW volume alerts
constexpr double MIN_REFERENCE_DB = -40.0;

// Delay before triggering alert (milliseconds)
constexpr int ALERT_DELAY_MS = 6000;

// Check interval for volume monitoring (milliseconds)
constexpr int VOLUME_CHECK_INTERVAL_MS = 1000;

// ============================================================================
// Animation Timing
// ============================================================================

// Warning blink interval (milliseconds)
constexpr int WARNING_BLINK_INTERVAL_MS = 400;

// ============================================================================
// Colors - Source Type Badges
// ============================================================================

const inline QColor COLOR_MIX_BADGE(0xFF, 0x6B, 0x35);         // Orange #FF6B35
const inline QColor COLOR_TRANSLATED_BADGE(0x4A, 0x90, 0xD9);  // Blue #4A90D9
const inline QColor COLOR_REFERENCE_BADGE(0x9B, 0x59, 0xB6);   // Purple #9B59B6

// ============================================================================
// Colors - Warning States
// ============================================================================

const inline QColor COLOR_WARNING_HIGH(255, 140, 0);   // Bright orange
const inline QColor COLOR_WARNING_LOW(255, 0, 0);      // Red

// ============================================================================
// Colors - Filter Levels
// ============================================================================

const inline QString COLOR_FILTER_OFF = "#B8B8B8";     // Muted gray
const inline QString COLOR_FILTER_LOW = "#37D247";     // Green (success)
const inline QString COLOR_FILTER_HIGH = "#E5AF24";    // Orange (warning)

// ============================================================================
// Colors - Threshold Controls (meter zones)
// ============================================================================

const inline QColor COLOR_THRESHOLD_NOMINAL(0x4C, 0xFF, 0x4C);   // Bright green
const inline QColor COLOR_THRESHOLD_WARNING(0xFF, 0xCC, 0x00);   // Orange/yellow
const inline QColor COLOR_THRESHOLD_ERROR(0xFF, 0x4C, 0x4C);     // Bright red

// ============================================================================
// Colors - UI Elements
// ============================================================================

const inline QString COLOR_HEADER_BG = "#272A33";
const inline QString COLOR_CONTAINER_BG = "#1D1F26";
const inline QString COLOR_BORDER = "#3C404D";
const inline QString COLOR_TEXT_MUTED = "#B8B8B8";
const inline QString COLOR_TEXT_SECONDARY = "#999999";

// Default source card color (dark brown, like KFS)
const inline QColor COLOR_DEFAULT_CARD(0x4E, 0x34, 0x2E);

// ============================================================================
// RNNoise Filter
// ============================================================================

// RNNoise filter ID (use ID for stability across renames)
constexpr const char *RNNOISE_FILTER_ID = "rnnoise_filter_sh_model";

// Maximum number of RNNoise filters per source
constexpr int MAX_RNNOISE_FILTERS = 2;

// Default filter strength (percentage)
constexpr double RNNOISE_DEFAULT_STRENGTH = 100.0;

} // namespace AudioMonitorConstants
