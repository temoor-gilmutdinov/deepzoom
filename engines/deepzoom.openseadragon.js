(function ($) {
  Drupal.behaviors.deepzoom = {
    attach: function (context) {
      $('.deepzoom', context).each(function () {
        id = $(this).attr('id').split('-');
        viewer = new OpenSeadragon.Viewer({
          id: id.join('-'),
          prefixUrl: Drupal.settings.deepZoom.library + '/images/',
          tileSources: Drupal.settings.deepZoom.path + '/' + id[1] + '.dzi',
          minZoomImageRatio: Drupal.settings.deepZoom.min_zoom_ratio,
          maxZoomPixelRatio: Drupal.settings.deepZoom.max_zoom_ratio
        });
      });
    }
  };
})(jQuery);
