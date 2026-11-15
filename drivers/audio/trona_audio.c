// SPDX-License-Identifier: GPL-2.0-only
/* Amazon Trona: MT8183 DL1 -> ADDA -> MT6358 analog outputs.
 * Wiring and amplifier timing from the shipped Amazon Trona device tree
 * and mtk-auddrv-gpio.c. Front/back-end structure follows mainline MT8183.
 */
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <sound/soc.h>

struct trona_audio { struct gpio_desc *amp, *gain; };
static int speaker_event(struct snd_soc_dapm_widget *w,
                         struct snd_kcontrol *k, int event)
{
 struct trona_audio *p = snd_soc_card_get_drvdata(w->dapm->card);
 if (SND_SOC_DAPM_EVENT_ON(event)) {
  /* Stock Trona gain mode 1 uses static GPIO, not pulse programming. */
  gpiod_set_value_cansleep(p->gain, 1);
  gpiod_set_value_cansleep(p->amp, 1);
  msleep(50);
 } else gpiod_set_value_cansleep(p->amp, 0);
 return 0;
}
static const struct snd_soc_dapm_widget widgets[] = {
 SND_SOC_DAPM_SPK("Speaker", speaker_event),
 SND_SOC_DAPM_HP("Headphone", NULL),
 SND_SOC_DAPM_MIC("Internal Mic", NULL),
};
/* Board speakers use the HPL/HPR outputs and external GPIO amplifier.
 * UCM selects Audio Playback to keep both DAC channels independent.
 * LoudSPK Playback activated the analog stages but lost a channel in
 * the physical left/right test; Audio Playback makes both tones audible.
 */
static const struct snd_soc_dapm_route routes[] = {
 {"Speaker", NULL, "Headphone L Ext Spk Amp"},
 {"Speaker", NULL, "Headphone R Ext Spk Amp"},
 {"Headphone", NULL, "Headphone L"},
 {"Headphone", NULL, "Headphone R"},
 {"AIN0", NULL, "Internal Mic"},
 {"AIN2", NULL, "Internal Mic"},
};
static const struct snd_kcontrol_new controls[] = {
 SOC_DAPM_PIN_SWITCH("Speaker"), SOC_DAPM_PIN_SWITCH("Headphone"),
 SOC_DAPM_PIN_SWITCH("Internal Mic"),
};
SND_SOC_DAILINK_DEFS(playback,
 DAILINK_COMP_ARRAY(COMP_CPU("DL1")),
 DAILINK_COMP_ARRAY(COMP_DUMMY()),
 DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture,
 DAILINK_COMP_ARRAY(COMP_CPU("UL1")),
 DAILINK_COMP_ARRAY(COMP_DUMMY()),
 DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(codec,
 DAILINK_COMP_ARRAY(COMP_CPU("ADDA")),
 DAILINK_COMP_ARRAY(COMP_CODEC("mt6358-sound", "mt6358-snd-codec-aif1")),
 DAILINK_COMP_ARRAY(COMP_EMPTY()));
static struct snd_soc_dai_link links[] = {
 { .name = "Playback", .stream_name = "Playback", .dynamic = 1,
   .playback_only = 1,
   .trigger = {SND_SOC_DPCM_TRIGGER_PRE, SND_SOC_DPCM_TRIGGER_PRE},
   SND_SOC_DAILINK_REG(playback) },
 { .name = "Capture", .stream_name = "Capture", .dynamic = 1,
   .capture_only = 1,
   .trigger = {SND_SOC_DPCM_TRIGGER_PRE, SND_SOC_DPCM_TRIGGER_PRE},
   SND_SOC_DAILINK_REG(capture) },
 { .name = "MT6358 Codec", .no_pcm = 1,
   SND_SOC_DAILINK_REG(codec) },
};
static int late_probe(struct snd_soc_card *card)
{
 /* UCM enables speaker output after setting the analog path and gain. */
 snd_soc_dapm_disable_pin(&card->dapm, "Speaker");
 snd_soc_dapm_disable_pin(&card->dapm, "Headphone");
 snd_soc_dapm_disable_pin(&card->dapm, "Internal Mic");
 return snd_soc_dapm_sync(&card->dapm);
}
static struct snd_soc_card card = {
 .name = "Trona", .owner = THIS_MODULE,
 .dai_link = links, .num_links = ARRAY_SIZE(links),
 .dapm_widgets = widgets, .num_dapm_widgets = ARRAY_SIZE(widgets),
 .dapm_routes = routes, .num_dapm_routes = ARRAY_SIZE(routes),
 .controls = controls, .num_controls = ARRAY_SIZE(controls),
 .late_probe = late_probe,
};
static void put_node(void *node) { of_node_put(node); }
static int trona_probe(struct platform_device *pdev)
{
 struct device_node *afe;
 struct trona_audio *p;
 int i, ret;
 if (!of_machine_is_compatible("amazon,trona")) return -ENODEV;
 afe = of_parse_phandle(pdev->dev.of_node, "mediatek,platform", 0);
 if (!afe) return -EINVAL;
 ret = devm_add_action_or_reset(&pdev->dev, put_node, afe);
 if (ret) return ret;
 p = devm_kzalloc(&pdev->dev, sizeof(*p), GFP_KERNEL);
 if (!p) return -ENOMEM;
 p->amp = devm_gpiod_get(&pdev->dev, "speaker-enable", GPIOD_OUT_LOW);
 if (IS_ERR(p->amp)) return dev_err_probe(&pdev->dev, PTR_ERR(p->amp), "speaker enable GPIO\n");
 p->gain = devm_gpiod_get(&pdev->dev, "speaker-gain", GPIOD_OUT_LOW);
 if (IS_ERR(p->gain)) return dev_err_probe(&pdev->dev, PTR_ERR(p->gain), "speaker gain GPIO\n");
 for (i = 0; i < ARRAY_SIZE(links); i++) links[i].platforms[0].of_node = afe;
 card.dev = &pdev->dev;
 snd_soc_card_set_drvdata(&card, p);
 return devm_snd_soc_register_card(&pdev->dev, &card);
}
static const struct of_device_id matches[] = {
 { .compatible = "amazon,trona-audio" }, {}
};
MODULE_DEVICE_TABLE(of, matches);
static struct platform_driver trona_driver = {
 .probe = trona_probe,
 .driver = { .name = "trona-audio", .of_match_table = matches },
};
module_platform_driver(trona_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Amazon Fire HD10 Trona analog audio machine driver");
