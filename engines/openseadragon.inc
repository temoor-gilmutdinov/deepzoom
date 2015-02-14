<?php
/**
 * @file
 * OpenSeadragon engine integration.
 */

/**
 * Implements deepzoom_engine_HOOK_info().
 */
function deepzoom_engine_openseadragon_info(&$items) {
  $items['openseadragon'] = array(
    'name' => t('OpenSeadragon Ajax'),
    'callbacks' => array(
      'defaults' => 'deepzoom_engine_openseadragon_defaults',
      'formatter settings' => 'deepzoom_engine_openseadragon_formatter_settings',
      'formatter summary' => 'deepzoom_engine_openseadragon_formatter_summary',
      'page alter' => 'deepzoom_engine_openseadragon_page_alter',
      'settings form' => 'deepzoom_engine_openseadragon_settings_form',
      'submit' => 'deepzoom_engine_openseadragon_submit',
    ),
  );
}

/**
 * Defaults callback for openseadragon Ajax engine.
 */
function deepzoom_engine_openseadragon_defaults() {
  $defaults = array(
    'tile_size' => 254,
    'tile_overlap' => 1,
    'max_zoom_ratio' => 1,
    'min_zoom_ratio' => 1,
  );
  return array_merge($defaults, variable_get('deepzoom_config', $defaults));
}

/**
 * Page alter callback for openseadragon Ajax engine.
 */
function deepzoom_engine_openseadragon_page_alter($vars) {
  global $base_url;
  $library = module_exists('libraries') ? libraries_get_path('openseadragon') : 'sites/all/libraries/openseadragon';

  drupal_add_js($library . '/openseadragon.min.js');
  drupal_add_js(drupal_get_path('module', 'deepzoom') . '/engines/deepzoom.openseadragon.js');

  $path = str_replace($base_url, '', file_create_url('public://deepzoom'));

  $global_config = variable_get('deepzoom_config');

  $settings = array(
    'library' => base_path() . $library,
    'path' => $path,
  );

  if (!empty($global_config['max_zoom_ratio'])) {
    $settings['max_zoom_ratio'] = $global_config['max_zoom_ratio'];
  }
  if (!empty($global_config['min_zoom_ratio'])) {
    $settings['min_zoom_ratio'] = $global_config['min_zoom_ratio'];
  }

  drupal_add_js(array('deepZoom' => $settings), 'setting');
}

/**
 * Settings form callback for openseadragon Ajax engine.
 */
function deepzoom_engine_openseadragon_settings_form($vars) {
  $form = array();

  $form['tile_size'] = array(
    '#title' => t('tile size'),
    '#type' => 'textfield',
    '#default_value' => $vars['config']['tile_size'],
    '#description' => t('tile size setting.'),
  );

  $form['max_zoom_ratio'] = array(
    '#title' => t('maxZoomPixelRatio'),
    '#description' => t('Limits maximum zoom in from original image perspective'),
    '#type' => 'textfield',
    '#default_value' => $vars['config']['max_zoom_ratio'],
  );

  $form['min_zoom_ratio'] = array(
    '#title' => t('minZoomImageRatio'),
    '#description' => t('Limits maximum zoom out from viewport perspective'),
    '#type' => 'textfield',
    '#default_value' => $vars['config']['min_zoom_ratio'],
  );

  return $form;
}
