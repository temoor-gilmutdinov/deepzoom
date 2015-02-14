<?php
/**
 * @file
 * System module integration.
 */

/**
 * Check for availability of DZI.
 *
 * @param string $fid
 *   File ID.
 *
 * @return mixed
 *   MD5 hash of file, if DZI exists, otherwise false.
 */
function deepzoom_image_get_info($fid) {
  $query = db_select('deepzoom', 'dz');

  $query->addField('dz', 'md5');
  $query->condition('fid', $fid);

  $res = $query->execute()->fetchAssoc();

  return empty($res['md5']) ? FALSE : $res['md5'];
}

/**
 * Establish relation between generated DZI and source file.
 *
 * @param string $fid
 *   File URI.
 * @param string $md5
 *   MD5 hash.
 *
 * @throws \Exception
 * @throws \InvalidMergeQueryException
 */
function deepzoom_image_register($fid, $md5) {
  $query = db_merge('deepzoom');
  $query->key(array('md5' => $md5));
  $query->fields(array(
    'md5' => $md5,
    'fid' => $fid,
  ));
  $query->execute();
}

/**
 * Deep Zoom processor - Memory Imagick.
 */
function deepzoom_generate_deepzoom_memory_imagick($images) {
  $generate_access = user_access('generate deepzoom files');
  $return = array();

  foreach ($images as $fid => $uri) {
    if (!$return[$fid] = deepzoom_image_get_info($fid)) {
      if ($generate_access) {
        $return[$fid] = deepzoom_generate_deepzoom_memory_imagick_process($uri);
        deepzoom_image_register($fid, $return[$fid]);
      }
    }
  }

  return $return;
}

/**
 * Deep Zoom processor - Memory Imagick process callback.
 */
function deepzoom_generate_deepzoom_memory_imagick_process($uri) {
  $start = microtime(TRUE);
  ini_set('max_execution_time', 600);
  $md5 = md5_file($uri);
  // Sometimes Imagick can't open uri, convert it to real path just in case.
  $image_base = new Imagick(drupal_realpath($uri));
  $image_info = deepzoom_generate_image_info_imagick($md5, NULL, $image_base);
  $deepzoom_config = deepzoom_config();
  $level = 0;
  $column = 0;
  $row = 0;
  $level_progress = 0;
  $total_progress = 0;

  while ($total_progress < $image_info['count']) {
    $level_dir = "public://deepzoom/{$image_info['md5']}_files/{$level}";

    // Setup level directory and temporary image.
    if ($level_progress == 0 && (!is_dir($level_dir) || !isset($resource_resize))) {
      if (file_prepare_directory($level_dir, FILE_CREATE_DIRECTORY)) {
        $dst_w = $image_info['levels'][$level]['width'];
        $dst_h = $image_info['levels'][$level]['height'];
        $src_w = $image_info['width'];
        $src_h = $image_info['height'];
        if ($src_h == $dst_h && $src_w == $dst_w) {
          $resource_resize = $image_base;
        }
        else {
          $resource_resize = clone $image_base;
          $resource_resize->scaleImage($dst_w, $dst_h);
        }
      }
    }

    // Tile processing.
    if (!file_exists("{$level_dir}/{$column}_{$row}.{$image_info['extension']}")) {
      $offset_x = $column == 0 ? 0 : $deepzoom_config['tile_overlap'];
      $offset_y = $row == 0 ? 0 : $deepzoom_config['tile_overlap'];
      $x = ($column * $deepzoom_config['tile_size']) - $offset_x;
      $y = ($row * $deepzoom_config['tile_size']) - $offset_y;

      $width = $deepzoom_config['tile_size'] + ($column == 0 ? 1 : 2) * $deepzoom_config['tile_overlap'];
      $height = $deepzoom_config['tile_size'] + ($row == 0 ? 1 : 2) * $deepzoom_config['tile_overlap'];
      $crop_width = min($width, $image_info['levels'][$level]['width'] - $x);
      $crop_height = min($height, $image_info['levels'][$level]['height'] - $y);

      $resource_crop = $resource_resize->getImageRegion($crop_width, $crop_height, $x, $y);
      $file = fopen("{$level_dir}/{$column}_{$row}.{$image_info['extension']}", 'w');
      $resource_crop->writeImageFile($file);
      $resource_crop->clear();
      fclose($file);
    }

    // Increment counters.
    $level_progress++;
    $total_progress++;

    // Increment rows and columns.
    $row++;
    if ($row >= $image_info['levels'][$level]['rows']) {
      $row = 0;
      $column++;
    }

    // Rollover level counter and cleanup.
    if ($level_progress >= $image_info['levels'][$level]['count']) {
      if (!empty($resource_resize)) {
        $resource_resize->clear();
        unset($resource_resize);
      }

      $row = 0;
      $column = 0;
      $level_progress = 0;
      $level++;
    }
  }

  // Finish and generate DZI file.
  file_unmanaged_save_data(theme('deepzoom_dzi', array(
    'size' => $deepzoom_config['tile_size'],
    'overlap' => $deepzoom_config['tile_overlap'],
    'format' => $image_info['extension'],
    'width' => $image_info['width'],
    'height' => $image_info['height'],
  )), "public://deepzoom/{$image_info['md5']}.dzi", FILE_EXISTS_REPLACE);

  $end = microtime(TRUE);
  if (variable_get('deepzoom_debug_mode', FALSE)) {
    watchdog('deepzoom', 'Imagagic processing time: @time', array('@time' => $end - $start), WATCHDOG_DEBUG);
  }

  return $md5;
}

/**
 * Deep Zoom processor - Memory.
 */
function deepzoom_generate_deepzoom_memory($images) {
  $generate_access = user_access('generate deepzoom files');
  $return = array();

  foreach ($images as $fid => $uri) {
    if (!$return[$fid] = deepzoom_image_get_info($fid)) {
      if ($generate_access) {
        $return[$fid] = deepzoom_generate_deepzoom_memory_process($uri);
        deepzoom_image_register($fid, $return[$fid]);
      }
    }
  }

  return $return;
}

/**
 * Deep Zoom processor - Memory process callback.
 */
function deepzoom_generate_deepzoom_memory_process($uri) {
  $start = microtime(TRUE);
  ini_set('max_execution_time', 600);
  $md5 = md5_file($uri);
  $image_base = image_load($uri);
  $image_info = deepzoom_generate_image_info($md5, NULL, $image_base);
  $deepzoom_config = deepzoom_config();
  $level = 0;
  $column = 0;
  $row = 0;
  $level_progress = 0;
  $total_progress = 0;

  while ($total_progress < $image_info['count']) {
    $level_dir = "public://deepzoom/{$image_info['md5']}_files/{$level}";

    // Setup level directory and temporary image.
    if ($level_progress == 0 && (!is_dir($level_dir) || !isset($resource_resize))) {
      if (file_prepare_directory($level_dir, FILE_CREATE_DIRECTORY)) {
        $dst_x = $dst_y = $src_x = $src_y = 0;
        $dst_w = $image_info['levels'][$level]['width'];
        $dst_h = $image_info['levels'][$level]['height'];
        $src_w = $image_info['width'];
        $src_h = $image_info['height'];
        if ($src_h == $dst_h && $src_w == $dst_w) {
          $resource_resize = $image_base->resource;
        }
        else {
          $resource_resize = imagecreatetruecolor($dst_w, $dst_h);
          imagecopyresampled($resource_resize, $image_base->resource, $dst_x, $dst_y, $src_x, $src_y, $dst_w, $dst_h, $src_w, $src_h);
        }
      }
    }

    // Tile processing.
    if (!file_exists("{$level_dir}/{$column}_{$row}.{$image_info['extension']}")) {
      $offset_x = $column == 0 ? 0 : $deepzoom_config['tile_overlap'];
      $offset_y = $row == 0 ? 0 : $deepzoom_config['tile_overlap'];
      $x = ($column * $deepzoom_config['tile_size']) - $offset_x;
      $y = ($row * $deepzoom_config['tile_size']) - $offset_y;

      $width = $deepzoom_config['tile_size'] + ($column == 0 ? 1 : 2) * $deepzoom_config['tile_overlap'];
      $height = $deepzoom_config['tile_size'] + ($row == 0 ? 1 : 2) * $deepzoom_config['tile_overlap'];
      $crop_width = min($width, $image_info['levels'][$level]['width'] - $x);
      $crop_height = min($height, $image_info['levels'][$level]['height'] - $y);

      $resource_crop = imagecreatetruecolor($crop_width, $crop_height);
      imagecopyresampled($resource_crop, $resource_resize, 0, 0, $x, $y, $crop_width, $crop_height, $crop_width, $crop_height);
      $image_clone = clone $image_base;
      $image_clone->resource = $resource_crop;
      image_save($image_clone, "{$level_dir}/{$column}_{$row}.{$image_info['extension']}");
      unset($image_clone);
      imagedestroy($resource_crop);
    }

    // Increment counters.
    $level_progress++;
    $total_progress++;

    // Incrememnt rows and columns.
    $row++;
    if ($row >= $image_info['levels'][$level]['rows']) {
      $row = 0;
      $column++;
    }

    // Rollover level counter and cleanup.
    if ($level_progress >= $image_info['levels'][$level]['count']) {
      if (!empty($resource_resize)) {
        imagedestroy($resource_resize);
        unset($resource_resize);
      }

      $row = 0;
      $column = 0;
      $level_progress = 0;
      $level++;
    }
  }

  // Finish and generate DZI file.
  file_unmanaged_save_data(theme('deepzoom_dzi', array(
    'size' => $deepzoom_config['tile_size'],
    'overlap' => $deepzoom_config['tile_overlap'],
    'format' => $image_info['extension'],
    'width' => $image_info['width'],
    'height' => $image_info['height'],
  )), "public://deepzoom/{$image_info['md5']}.dzi", FILE_EXISTS_REPLACE);

  $end = microtime(TRUE);
  if (variable_get('deepzoom_debug_mode', FALSE)) {
    watchdog('deepzoom', 'Imagagic processing time: @time', array('@time' => $end - $start), WATCHDOG_DEBUG);
  }

  return $md5;
}

/**
 * Deep Zoom processor - Batch API.
 */
function deepzoom_generate_deepzoom_batch($images) {
  $return = array();

  $batch = array(
    'title' => t('Generating Deep Zoom files'),
    'operations' => array(),
  );
  foreach ($images as $fid => $uri) {
    if (!$return[$fid] = deepzoom_image_get_info($fid)) {
      $batch['operations'][] = array(
        'deepzoom_generate_deepzoom_batch_process',
        array($uri, $fid),
      );
    }
  }

  if (count($batch['operations']) > 0 && user_access('generate deepzoom files')) {
    batch_set($batch);
    batch_process();
  }

  return $return;
}

/**
 * Deep Zoom processor - Batch API process callback.
 */
function deepzoom_generate_deepzoom_batch_process($uri, $fid, &$context) {
  if (empty($context['sandbox'])) {
    $md5 = md5_file($uri);
    $context['sandbox']['image'] = deepzoom_generate_image_info($md5, $uri);
    $context['sandbox']['config'] = deepzoom_config();
    $context['sandbox']['level'] = 0;
    $context['sandbox']['column'] = 0;
    $context['sandbox']['row'] = 0;
    $context['sandbox']['level_progress'] = 0;
    $context['sandbox']['total_progress'] = 0;
    $context['sandbox']['time_debug'] = 0;
  }

  $md5 = $context['sandbox']['image']['md5'];
  $count = 0;
  $max = 10;
  while ($count < $max && $context['sandbox']['total_progress'] < $context['sandbox']['image']['count']) {
    $level_dir = "public://deepzoom/{$context['sandbox']['image']['md5']}_files/{$context['sandbox']['level']}";

    // Setup level directory and temporary image.
    if ($context['sandbox']['level_progress'] == 0 && !is_dir($level_dir)) {
      if (file_prepare_directory($level_dir, FILE_CREATE_DIRECTORY)) {
        $image = image_load($uri);
        image_effect_apply($image, array(
          'effect callback' => 'image_resize_effect',
          'data' => array(
            'width' => $context['sandbox']['image']['levels'][$context['sandbox']['level']]['width'],
            'height' => $context['sandbox']['image']['levels'][$context['sandbox']['level']]['height'],
          ),
        ));
        image_save($image, "{$level_dir}/._temp.{$context['sandbox']['image']['extension']}");
      }
    }

    // Tile processing.
    // @TODO - Make a seperate function for this.
    if (!file_exists("{$level_dir}/{$context['sandbox']['column']}_{$context['sandbox']['row']}.{$context['sandbox']['image']['extension']}")) {
      $offset_x = $context['sandbox']['column'] == 0 ? 0 : $context['sandbox']['config']['tile_overlap'];
      $offset_y = $context['sandbox']['row'] == 0 ? 0 : $context['sandbox']['config']['tile_overlap'];
      $x = ($context['sandbox']['column'] * $context['sandbox']['config']['tile_size']) - $offset_x;
      $y = ($context['sandbox']['row'] * $context['sandbox']['config']['tile_size']) - $offset_y;

      $width = $context['sandbox']['config']['tile_size'] + ($context['sandbox']['column'] == 0 ? 1 : 2) * $context['sandbox']['config']['tile_overlap'];
      $height = $context['sandbox']['config']['tile_size'] + ($context['sandbox']['row'] == 0 ? 1 : 2) * $context['sandbox']['config']['tile_overlap'];
      $crop_width = min($width, $context['sandbox']['image']['levels'][$context['sandbox']['level']]['width'] - $x);
      $crop_height = min($height, $context['sandbox']['image']['levels'][$context['sandbox']['level']]['height'] - $y);

      $temp_image = image_load("{$level_dir}/._temp.{$context['sandbox']['image']['extension']}");
      image_effect_apply($temp_image, array(
        'effect callback' => 'image_crop_effect',
        'data' => array(
          'anchor' => "{$x}-{$y}",
          'width' => $crop_width,
          'height' => $crop_height,
        ),
      ));
      image_save($temp_image, "{$level_dir}/{$context['sandbox']['column']}_{$context['sandbox']['row']}.{$context['sandbox']['image']['extension']}");
      unset($temp_image);
    }

    // Increment counters.
    $count++;
    $context['sandbox']['level_progress']++;
    $context['sandbox']['total_progress']++;

    // Incrememnt rows and columns.
    $context['sandbox']['row']++;
    if ($context['sandbox']['row'] >= $context['sandbox']['image']['levels'][$context['sandbox']['level']]['rows']) {
      $context['sandbox']['row'] = 0;
      $context['sandbox']['column']++;
    }

    // Rollover level counter and cleanup.
    if ($context['sandbox']['level_progress'] >= $context['sandbox']['image']['levels'][$context['sandbox']['level']]['count']) {
      file_unmanaged_delete("{$level_dir}/._temp.{$context['sandbox']['image']['extension']}");

      $context['sandbox']['row'] = 0;
      $context['sandbox']['column'] = 0;
      $context['sandbox']['level_progress'] = 0;
      $context['sandbox']['level']++;
    }
  }

  // Set progress percentage.
  if ($context['sandbox']['total_progress'] != $context['sandbox']['image']['count']) {
    $context['finished'] = $context['sandbox']['total_progress'] / $context['sandbox']['image']['count'];
  }
  // Finish and generate DZI file.
  else {
    file_unmanaged_save_data(theme('deepzoom_dzi', array(
      'size' => $context['sandbox']['config']['tile_size'],
      'overlap' => $context['sandbox']['config']['tile_overlap'],
      'format' => $context['sandbox']['image']['extension'],
      'width' => $context['sandbox']['image']['width'],
      'height' => $context['sandbox']['image']['height'],
    )), "public://deepzoom/{$context['sandbox']['image']['md5']}.dzi", FILE_EXISTS_REPLACE);

    $context['finished'] = 1;

    deepzoom_image_register($fid, $md5);
  }
}
